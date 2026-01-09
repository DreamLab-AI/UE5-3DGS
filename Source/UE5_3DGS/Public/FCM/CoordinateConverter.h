// Copyright 2024 3DGS Research Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Coordinate system conversion utilities for UE5 to COLMAP/3DGS transformation.
 *
 * Coordinate Systems:
 * - UE5: Left-handed, Z-up (X=Forward, Y=Right, Z=Up)
 * - COLMAP: Right-handed, Y-down (X=Right, Y=Down, Z=Forward)
 * - OpenCV: Right-handed, Y-down (same as COLMAP)
 *
 * The conversion involves:
 * 1. Axis remapping: UE5(X,Y,Z) -> COLMAP(Y,-Z,X)
 * 2. Rotation quaternion transformation
 * 3. Scale factor application (UE5 uses centimeters, COLMAP uses meters)
 */
namespace UE5_3DGS
{
	/**
	 * Coordinate conversion constants and utilities
	 */
	class UNREALTOGAUSSIAN_API FCoordinateConverter
	{
	public:
		/** Scale factor: UE5 centimeters to COLMAP meters */
		static constexpr double CmToMeters = 0.01;
		static constexpr double MetersToCm = 100.0;

		/**
		 * Convert UE5 position (cm, left-handed Z-up) to COLMAP position (m, right-handed Y-down)
		 *
		 * @param UE5Position Position in Unreal Engine coordinates (centimeters)
		 * @return Position in COLMAP coordinates (meters)
		 */
		static FVector ConvertPositionToColmap(const FVector& UE5Position);

		/**
		 * Convert COLMAP position to UE5 position
		 *
		 * @param ColmapPosition Position in COLMAP coordinates (meters)
		 * @return Position in Unreal Engine coordinates (centimeters)
		 */
		static FVector ConvertPositionFromColmap(const FVector& ColmapPosition);

		/**
		 * Convert UE5 rotation to COLMAP quaternion (camera-to-world convention)
		 *
		 * COLMAP uses world-to-camera rotation (qw, qx, qy, qz format)
		 * The quaternion represents the rotation from world to camera frame.
		 *
		 * @param UE5Rotation Rotation in Unreal Engine coordinates
		 * @return Quaternion in COLMAP format (world-to-camera)
		 */
		static FQuat ConvertRotationToColmap(const FRotator& UE5Rotation);

		/**
		 * Convert UE5 quaternion to COLMAP quaternion
		 *
		 * @param UE5Quat Quaternion in Unreal Engine coordinates
		 * @return Quaternion in COLMAP format
		 */
		static FQuat ConvertQuatToColmap(const FQuat& UE5Quat);

		/**
		 * Convert COLMAP rotation to UE5 rotation
		 *
		 * @param ColmapQuat Quaternion in COLMAP format
		 * @return Rotation in Unreal Engine coordinates
		 */
		static FRotator ConvertRotationFromColmap(const FQuat& ColmapQuat);

		/**
		 * Convert UE5 direction vector to COLMAP direction
		 *
		 * @param UE5Direction Direction vector in UE5 coordinates
		 * @return Direction vector in COLMAP coordinates (normalized)
		 */
		static FVector ConvertDirectionToColmap(const FVector& UE5Direction);

		/**
		 * Get the transformation matrix from UE5 to COLMAP coordinate system
		 *
		 * @return 4x4 transformation matrix
		 */
		static FMatrix GetUE5ToColmapMatrix();

		/**
		 * Get the transformation matrix from COLMAP to UE5 coordinate system
		 *
		 * @return 4x4 transformation matrix
		 */
		static FMatrix GetColmapToUE5Matrix();

		/**
		 * Convert a full camera transform (position + rotation) to COLMAP format
		 * Returns the camera extrinsic matrix in COLMAP convention.
		 *
		 * @param CameraTransform Full transform of the camera in UE5
		 * @param OutPosition Output position in COLMAP coordinates
		 * @param OutRotation Output quaternion in COLMAP format (world-to-camera)
		 */
		static void ConvertCameraToColmap(
			const FTransform& CameraTransform,
			FVector& OutPosition,
			FQuat& OutRotation
		);

		/**
		 * Compute the camera center from COLMAP extrinsics
		 * COLMAP stores world-to-camera, so camera center = -R^T * t
		 *
		 * @param ColmapRotation Quaternion in COLMAP format
		 * @param ColmapTranslation Translation in COLMAP format
		 * @return Camera center in world coordinates
		 */
		static FVector ComputeCameraCenter(
			const FQuat& ColmapRotation,
			const FVector& ColmapTranslation
		);

	private:
		/** Axis swap matrix: UE5 -> COLMAP */
		static const FMatrix AxisSwapMatrix;

		/** Inverse axis swap matrix: COLMAP -> UE5 */
		static const FMatrix InverseAxisSwapMatrix;
	};

	/**
	 * Specialized converter for Gaussian Splatting PLY format
	 * Handles the specific coordinate conventions used in 3DGS training
	 */
	class UNREALTOGAUSSIAN_API FGaussianCoordinateConverter
	{
	public:
		/**
		 * Convert UE5 position to 3DGS PLY format position
		 * 3DGS uses the same coordinate system as COLMAP
		 *
		 * @param UE5Position Position in UE5 coordinates
		 * @return Position suitable for PLY export
		 */
		static FVector ConvertPositionToPLY(const FVector& UE5Position);

		/**
		 * Convert UE5 rotation to spherical harmonics orientation
		 * Used for the rotation component of gaussians
		 *
		 * @param UE5Rotation Rotation in UE5 coordinates
		 * @return Quaternion for gaussian orientation (x,y,z,w format in PLY)
		 */
		static FQuat ConvertRotationToPLY(const FRotator& UE5Rotation);

		/**
		 * Convert scale factors for gaussian ellipsoids
		 * Applies coordinate system transformation and unit conversion
		 *
		 * @param UE5Scale Scale in UE5 units (centimeters)
		 * @return Scale in meters for PLY format
		 */
		static FVector ConvertScaleToPLY(const FVector& UE5Scale);
	};
}
