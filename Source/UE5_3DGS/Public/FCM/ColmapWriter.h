// Copyright 2024 3DGS Research Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FCM/CameraIntrinsics.h"
#include "SCM/CameraTrajectory.h"

namespace UE5_3DGS
{
	/**
	 * Camera data for COLMAP export
	 */
	struct UNREALTOGAUSSIAN_API FColmapCamera
	{
		/** Unique camera ID */
		int32 CameraId = 1;

		/** Camera intrinsics */
		FCameraIntrinsics Intrinsics;

		/** Whether this is a shared camera model */
		bool bIsShared = true;

		/** Camera model name (for convenience) */
		FString Model;

		/** Image width */
		int32 Width = 0;

		/** Image height */
		int32 Height = 0;

		/** Parameters string (for convenience) */
		FString Params;
	};

	/**
	 * Image data for COLMAP export
	 */
	struct UNREALTOGAUSSIAN_API FColmapImage
	{
		/** Unique image ID */
		int32 ImageId = 0;

		/** Camera ID this image uses */
		int32 CameraId = 1;

		/** Image filename (relative path) */
		FString ImageName;

		/** Camera rotation quaternion (COLMAP format: qw, qx, qy, qz) */
		FQuat Rotation = FQuat::Identity;

		/** Camera translation (COLMAP format: world-to-camera) */
		FVector Translation = FVector::ZeroVector;

		/** 2D keypoints for this image (optional, for feature matching) */
		TArray<FVector2D> Keypoints;
	};

	/**
	 * 3D point data for COLMAP export
	 */
	struct UNREALTOGAUSSIAN_API FColmapPoint3D
	{
		/** Unique point ID */
		int64 PointId = 0;

		/** 3D position in COLMAP coordinates */
		FVector Position = FVector::ZeroVector;

		/** RGB color */
		FColor Color = FColor::White;

		/** Reconstruction error */
		float Error = 0.0f;

		/** Image IDs where this point is visible */
		TArray<int32> ImageIds;

		/** 2D keypoint indices in each image */
		TArray<int32> Point2DIndices;
	};

	/**
	 * COLMAP format writer for 3DGS training data export
	 *
	 * Supports both text and binary COLMAP formats.
	 * Output structure:
	 *   sparse/0/
	 *     cameras.txt / cameras.bin
	 *     images.txt / images.bin
	 *     points3D.txt / points3D.bin
	 *   images/
	 *     image_00000.jpg
	 *     ...
	 */
	class UNREALTOGAUSSIAN_API FColmapWriter
	{
	public:
		/**
		 * Write complete COLMAP dataset
		 *
		 * @param OutputDir Base output directory
		 * @param Cameras Camera models
		 * @param Images Image data with poses
		 * @param Points3D 3D points (can be empty for initial training)
		 * @param bBinary Write binary format (faster) or text (human-readable)
		 * @return True if successful
		 */
		static bool WriteColmapDataset(
			const FString& OutputDir,
			const TArray<FColmapCamera>& Cameras,
			const TArray<FColmapImage>& Images,
			const TArray<FColmapPoint3D>& Points3D,
			bool bBinary = false
		);

		/**
		 * Write cameras file
		 *
		 * @param FilePath Output path
		 * @param Cameras Camera models
		 * @param bBinary Binary or text format
		 * @return True if successful
		 */
		static bool WriteCameras(
			const FString& FilePath,
			const TArray<FColmapCamera>& Cameras,
			bool bBinary = false
		);

		/**
		 * Write images file
		 *
		 * @param FilePath Output path
		 * @param Images Image data
		 * @param bBinary Binary or text format
		 * @return True if successful
		 */
		static bool WriteImages(
			const FString& FilePath,
			const TArray<FColmapImage>& Images,
			bool bBinary = false
		);

		/**
		 * Write points3D file
		 *
		 * @param FilePath Output path
		 * @param Points 3D points
		 * @param bBinary Binary or text format
		 * @return True if successful
		 */
		static bool WritePoints3D(
			const FString& FilePath,
			const TArray<FColmapPoint3D>& Points,
			bool bBinary = false
		);

		/**
		 * Create COLMAP images from viewpoints and intrinsics
		 *
		 * @param Viewpoints Camera viewpoints
		 * @param Intrinsics Camera intrinsics
		 * @param ImagePrefix Filename prefix (e.g., "image_")
		 * @param ImageExtension File extension (e.g., ".jpg")
		 * @return Array of COLMAP images
		 */
		static TArray<FColmapImage> CreateImagesFromViewpoints(
			const TArray<FCameraViewpoint>& Viewpoints,
			const FCameraIntrinsics& Intrinsics,
			const FString& ImagePrefix = TEXT("image_"),
			const FString& ImageExtension = TEXT(".jpg")
		);

		/**
		 * Create COLMAP camera from intrinsics
		 *
		 * @param Intrinsics Camera intrinsics
		 * @param CameraId Camera ID (default 1)
		 * @return COLMAP camera structure
		 */
		static FColmapCamera CreateCamera(
			const FCameraIntrinsics& Intrinsics,
			int32 CameraId = 1
		);

		/**
		 * Create directory structure for COLMAP dataset
		 *
		 * @param OutputDir Base directory
		 * @return True if directories created successfully
		 */
		static bool CreateDirectoryStructure(const FString& OutputDir);

		/**
		 * Validate COLMAP dataset
		 *
		 * @param OutputDir Dataset directory
		 * @param OutWarnings Output warnings
		 * @return True if valid
		 */
		static bool ValidateDataset(
			const FString& OutputDir,
			TArray<FString>& OutWarnings
		);

	private:
		// Text format writers
		static bool WriteCamerasText(const FString& FilePath, const TArray<FColmapCamera>& Cameras);
		static bool WriteImagesText(const FString& FilePath, const TArray<FColmapImage>& Images);
		static bool WritePoints3DText(const FString& FilePath, const TArray<FColmapPoint3D>& Points);

		// Binary format writers
		static bool WriteCamerasBinary(const FString& FilePath, const TArray<FColmapCamera>& Cameras);
		static bool WriteImagesBinary(const FString& FilePath, const TArray<FColmapImage>& Images);
		static bool WritePoints3DBinary(const FString& FilePath, const TArray<FColmapPoint3D>& Points);

		// Helper for padding image ID to fixed width
		static FString FormatImageIndex(int32 Index, int32 NumDigits = 5);
	};
}
