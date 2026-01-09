// Copyright 2024 3DGS Research Team. All Rights Reserved.

#include "FCM/CoordinateConverter.h"

namespace UE5_3DGS
{
	// Static matrix initialization
	// UE5: X=Forward, Y=Right, Z=Up (left-handed)
	// COLMAP: X=Right, Y=Down, Z=Forward (right-handed)
	// Transformation: COLMAP_X = UE5_Y, COLMAP_Y = -UE5_Z, COLMAP_Z = UE5_X
	const FMatrix FCoordinateConverter::AxisSwapMatrix = FMatrix(
		FPlane(0.0, 1.0, 0.0, 0.0),   // COLMAP X = UE5 Y
		FPlane(0.0, 0.0, -1.0, 0.0),  // COLMAP Y = -UE5 Z
		FPlane(1.0, 0.0, 0.0, 0.0),   // COLMAP Z = UE5 X
		FPlane(0.0, 0.0, 0.0, 1.0)
	);

	// Inverse transformation: UE5_X = COLMAP_Z, UE5_Y = COLMAP_X, UE5_Z = -COLMAP_Y
	const FMatrix FCoordinateConverter::InverseAxisSwapMatrix = FMatrix(
		FPlane(0.0, 0.0, 1.0, 0.0),   // UE5 X = COLMAP Z
		FPlane(1.0, 0.0, 0.0, 0.0),   // UE5 Y = COLMAP X
		FPlane(0.0, -1.0, 0.0, 0.0),  // UE5 Z = -COLMAP Y
		FPlane(0.0, 0.0, 0.0, 1.0)
	);

	FVector FCoordinateConverter::ConvertPositionToColmap(const FVector& UE5Position)
	{
		// Apply axis swap and unit conversion (cm -> m)
		// COLMAP: X=Right, Y=Down, Z=Forward
		// UE5: X=Forward, Y=Right, Z=Up
		return FVector(
			UE5Position.Y * CmToMeters,   // COLMAP X = UE5 Y (Right)
			-UE5Position.Z * CmToMeters,  // COLMAP Y = -UE5 Z (Down, negated because UE5 Z is Up)
			UE5Position.X * CmToMeters    // COLMAP Z = UE5 X (Forward)
		);
	}

	FVector FCoordinateConverter::ConvertPositionFromColmap(const FVector& ColmapPosition)
	{
		// Inverse transformation (m -> cm)
		return FVector(
			ColmapPosition.Z * MetersToCm,  // UE5 X = COLMAP Z
			ColmapPosition.X * MetersToCm,  // UE5 Y = COLMAP X
			-ColmapPosition.Y * MetersToCm  // UE5 Z = -COLMAP Y
		);
	}

	FQuat FCoordinateConverter::ConvertRotationToColmap(const FRotator& UE5Rotation)
	{
		return ConvertQuatToColmap(UE5Rotation.Quaternion());
	}

	FQuat FCoordinateConverter::ConvertQuatToColmap(const FQuat& UE5Quat)
	{
		// Convert quaternion from UE5 to COLMAP coordinate system
		// This involves:
		// 1. Converting the rotation axes
		// 2. Handling the handedness change

		// First, get the rotation matrix in UE5 coordinates
		FMatrix UE5RotMatrix = FRotationMatrix::Make(UE5Quat);

		// Apply coordinate transformation
		// Result = AxisSwap * UE5Rot * AxisSwap^(-1)
		// But for camera-to-world, we need the inverse (world-to-camera)
		FMatrix ColmapRotMatrix = AxisSwapMatrix * UE5RotMatrix * InverseAxisSwapMatrix;

		// COLMAP stores world-to-camera rotation, UE5 camera looks down +X
		// We need to account for the camera looking direction
		FMatrix CameraCorrection = FRotationMatrix::MakeFromX(FVector::ForwardVector);
		ColmapRotMatrix = ColmapRotMatrix * CameraCorrection;

		// Extract quaternion and ensure it's normalized
		FQuat Result = ColmapRotMatrix.ToQuat();
		Result.Normalize();

		// COLMAP uses (qw, qx, qy, qz) ordering, which is standard
		// FQuat stores as (X, Y, Z, W), so we return as-is
		return Result;
	}

	FRotator FCoordinateConverter::ConvertRotationFromColmap(const FQuat& ColmapQuat)
	{
		// Inverse of ConvertQuatToColmap
		FMatrix ColmapRotMatrix = FRotationMatrix::Make(ColmapQuat);

		// Remove camera correction
		FMatrix CameraCorrection = FRotationMatrix::MakeFromX(FVector::ForwardVector);
		ColmapRotMatrix = ColmapRotMatrix * CameraCorrection.Inverse();

		// Apply inverse coordinate transformation
		FMatrix UE5RotMatrix = InverseAxisSwapMatrix * ColmapRotMatrix * AxisSwapMatrix;

		return UE5RotMatrix.Rotator();
	}

	FVector FCoordinateConverter::ConvertDirectionToColmap(const FVector& UE5Direction)
	{
		FVector Result(
			UE5Direction.Y,   // COLMAP X = UE5 Y
			-UE5Direction.Z,  // COLMAP Y = -UE5 Z
			UE5Direction.X    // COLMAP Z = UE5 X
		);
		return Result.GetSafeNormal();
	}

	FMatrix FCoordinateConverter::GetUE5ToColmapMatrix()
	{
		// Include scale factor for position conversion
		FMatrix ScaleMatrix = FScaleMatrix(FVector(CmToMeters));
		return AxisSwapMatrix * ScaleMatrix;
	}

	FMatrix FCoordinateConverter::GetColmapToUE5Matrix()
	{
		FMatrix ScaleMatrix = FScaleMatrix(FVector(MetersToCm));
		return InverseAxisSwapMatrix * ScaleMatrix;
	}

	void FCoordinateConverter::ConvertCameraToColmap(
		const FTransform& CameraTransform,
		FVector& OutPosition,
		FQuat& OutRotation)
	{
		// Convert position
		OutPosition = ConvertPositionToColmap(CameraTransform.GetLocation());

		// Convert rotation (camera-to-world in UE5 -> world-to-camera in COLMAP)
		FQuat CameraToWorld = ConvertQuatToColmap(CameraTransform.GetRotation());

		// COLMAP stores world-to-camera, so we need the inverse
		OutRotation = CameraToWorld.Inverse();
		OutRotation.Normalize();
	}

	FVector FCoordinateConverter::ComputeCameraCenter(
		const FQuat& ColmapRotation,
		const FVector& ColmapTranslation)
	{
		// Camera center in world coordinates: C = -R^T * t
		// Where R is the rotation matrix and t is the translation
		FMatrix RotMatrix = FRotationMatrix::Make(ColmapRotation);
		FMatrix RotTranspose = RotMatrix.GetTransposed();

		FVector NegTranslation = -ColmapTranslation;
		return RotTranspose.TransformVector(NegTranslation);
	}

	// FGaussianCoordinateConverter implementation

	FVector FGaussianCoordinateConverter::ConvertPositionToPLY(const FVector& UE5Position)
	{
		// 3DGS PLY uses same coordinate system as COLMAP
		return FCoordinateConverter::ConvertPositionToColmap(UE5Position);
	}

	FQuat FGaussianCoordinateConverter::ConvertRotationToPLY(const FRotator& UE5Rotation)
	{
		// For gaussian splatting, the rotation defines the ellipsoid orientation
		// This is different from camera rotation - it's the local frame of the gaussian
		FQuat UE5Quat = UE5Rotation.Quaternion();

		// Apply coordinate transformation
		// Gaussian orientation in PLY: defines the axes of the 3D ellipsoid
		FQuat Result(
			UE5Quat.Y,   // PLY X = UE5 Y
			-UE5Quat.Z,  // PLY Y = -UE5 Z
			UE5Quat.X,   // PLY Z = UE5 X
			UE5Quat.W    // W stays the same
		);

		Result.Normalize();
		return Result;
	}

	FVector FGaussianCoordinateConverter::ConvertScaleToPLY(const FVector& UE5Scale)
	{
		// Scale conversion with axis swap and unit conversion
		// The scale defines the size of the gaussian along each axis
		return FVector(
			UE5Scale.Y * FCoordinateConverter::CmToMeters,  // PLY X scale = UE5 Y scale
			UE5Scale.Z * FCoordinateConverter::CmToMeters,  // PLY Y scale = UE5 Z scale (no negation for scale)
			UE5Scale.X * FCoordinateConverter::CmToMeters   // PLY Z scale = UE5 X scale
		);
	}
}
