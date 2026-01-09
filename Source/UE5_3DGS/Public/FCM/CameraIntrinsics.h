// Copyright 2024 3DGS Research Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CameraIntrinsics.generated.h"

/**
 * Camera intrinsic parameters for COLMAP compatibility.
 *
 * Supports multiple camera models:
 * - SIMPLE_PINHOLE: f, cx, cy (3 params)
 * - PINHOLE: fx, fy, cx, cy (4 params)
 * - SIMPLE_RADIAL: f, cx, cy, k1 (4 params)
 * - RADIAL: f, cx, cy, k1, k2 (5 params)
 * - OPENCV: fx, fy, cx, cy, k1, k2, p1, p2 (8 params)
 * - FULL_OPENCV: fx, fy, cx, cy, k1, k2, p1, p2, k3, k4, k5, k6 (12 params)
 */
UENUM(BlueprintType)
enum class EColmapCameraModel : uint8
{
	SIMPLE_PINHOLE = 0,
	PINHOLE = 1,
	SIMPLE_RADIAL = 2,
	RADIAL = 3,
	OPENCV = 4,
	FULL_OPENCV = 6
};

/**
 * Camera intrinsic parameters structure
 */
USTRUCT(BlueprintType)
struct UNREALTOGAUSSIAN_API FCameraIntrinsics
{
	GENERATED_BODY()

	/** Image width in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	int32 Width = 1920;

	/** Image height in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	int32 Height = 1080;

	/** Focal length X (in pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	double FocalLengthX = 0.0;

	/** Focal length Y (in pixels) - same as FocalLengthX for square pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	double FocalLengthY = 0.0;

	/** Principal point X (typically Width/2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	double PrincipalPointX = 0.0;

	/** Principal point Y (typically Height/2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	double PrincipalPointY = 0.0;

	/** Radial distortion coefficient k1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	double K1 = 0.0;

	/** Radial distortion coefficient k2 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	double K2 = 0.0;

	/** Tangential distortion coefficient p1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	double P1 = 0.0;

	/** Tangential distortion coefficient p2 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	double P2 = 0.0;

	/** Camera model type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	EColmapCameraModel CameraModel = EColmapCameraModel::PINHOLE;

	/** Default constructor */
	FCameraIntrinsics() = default;

	/** Constructor with resolution and FOV */
	FCameraIntrinsics(int32 InWidth, int32 InHeight, float HorizontalFOVDegrees);

	/** Get focal length from horizontal FOV */
	static double ComputeFocalLengthFromFOV(double FOVDegrees, double ImageDimension);

	/** Get horizontal FOV from focal length */
	static double ComputeFOVFromFocalLength(double FocalLength, double ImageDimension);

	/** Check if the intrinsics are valid */
	bool IsValid() const;

	/** Get the aspect ratio */
	double GetAspectRatio() const { return static_cast<double>(Width) / Height; }

	/** Get intrinsics as a 3x3 matrix K */
	FMatrix GetIntrinsicMatrix() const;

	/** Get COLMAP parameter string based on camera model */
	FString GetColmapParamsString() const;

	/** Get COLMAP camera model ID */
	int32 GetColmapModelId() const;

	/** Get COLMAP camera model name */
	FString GetColmapModelName() const;
};

namespace UE5_3DGS
{
	/**
	 * Utility class for camera intrinsics computation
	 */
	class UNREALTOGAUSSIAN_API FCameraIntrinsicsComputer
	{
	public:
		/**
		 * Compute intrinsics from a UE5 CineCameraActor or CameraComponent
		 *
		 * @param CameraComponent The camera component to extract settings from
		 * @param ImageWidth Output image width
		 * @param ImageHeight Output image height
		 * @return Computed camera intrinsics
		 */
		static FCameraIntrinsics ComputeFromCameraComponent(
			const class UCameraComponent* CameraComponent,
			int32 ImageWidth,
			int32 ImageHeight
		);

		/**
		 * Compute intrinsics from a SceneCaptureComponent2D
		 *
		 * @param CaptureComponent The scene capture component
		 * @param RenderTarget The render target (provides resolution)
		 * @return Computed camera intrinsics
		 */
		static FCameraIntrinsics ComputeFromSceneCaptureComponent(
			const class USceneCaptureComponent2D* CaptureComponent,
			const class UTextureRenderTarget2D* RenderTarget
		);

		/**
		 * Compute intrinsics from manual parameters
		 *
		 * @param HorizontalFOVDegrees Horizontal field of view in degrees
		 * @param ImageWidth Image width in pixels
		 * @param ImageHeight Image height in pixels
		 * @param CameraModel COLMAP camera model to use
		 * @return Computed camera intrinsics
		 */
		static FCameraIntrinsics ComputeFromFOV(
			float HorizontalFOVDegrees,
			int32 ImageWidth,
			int32 ImageHeight,
			EColmapCameraModel CameraModel = EColmapCameraModel::PINHOLE
		);

		/**
		 * Compute intrinsics from sensor size and focal length (CineCameraActor style)
		 *
		 * @param SensorWidthMM Sensor width in millimeters
		 * @param SensorHeightMM Sensor height in millimeters
		 * @param FocalLengthMM Focal length in millimeters
		 * @param ImageWidth Output image width in pixels
		 * @param ImageHeight Output image height in pixels
		 * @return Computed camera intrinsics
		 */
		static FCameraIntrinsics ComputeFromSensorAndFocalLength(
			float SensorWidthMM,
			float SensorHeightMM,
			float FocalLengthMM,
			int32 ImageWidth,
			int32 ImageHeight
		);

		/**
		 * Validate intrinsics for 3DGS training compatibility
		 *
		 * @param Intrinsics The intrinsics to validate
		 * @param OutWarnings Output array of warning messages
		 * @return True if valid for 3DGS training
		 */
		static bool ValidateFor3DGS(
			const FCameraIntrinsics& Intrinsics,
			TArray<FString>& OutWarnings
		);
	};
}
