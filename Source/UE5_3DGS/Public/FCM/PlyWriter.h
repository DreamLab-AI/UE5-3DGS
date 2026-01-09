// Copyright 2024 3DGS Research Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE5_3DGS
{
	/**
	 * Gaussian splat data for PLY export
	 *
	 * PLY format for 3DGS (236 bytes per splat):
	 * - Position: x, y, z (float32 x 3 = 12 bytes)
	 * - Normal: nx, ny, nz (float32 x 3 = 12 bytes)
	 * - DC SH coefficients: f_dc_0, f_dc_1, f_dc_2 (float32 x 3 = 12 bytes)
	 * - Rest SH coefficients: f_rest_0..f_rest_44 (float32 x 45 = 180 bytes)
	 * - Opacity: opacity (float32 = 4 bytes)
	 * - Scale: scale_0, scale_1, scale_2 (float32 x 3 = 12 bytes)
	 * - Rotation: rot_0, rot_1, rot_2, rot_3 (float32 x 4 = 16 bytes)
	 */
	struct UNREALTOGAUSSIAN_API FGaussianSplat
	{
		/** Position in world coordinates (meters for COLMAP compatibility) */
		FVector Position = FVector::ZeroVector;

		/** Surface normal */
		FVector Normal = FVector::UpVector;

		/** DC spherical harmonics coefficients (RGB color base) */
		FVector SH_DC = FVector(0.5f, 0.5f, 0.5f);

		/** Higher-order SH coefficients (45 values for order 3) */
		TArray<float> SH_Rest;

		/** Opacity (0-1) */
		float Opacity = 1.0f;

		/** Scale of the gaussian ellipsoid (log-space) */
		FVector Scale = FVector(-5.0f, -5.0f, -5.0f); // log(0.007) ~= -5

		/** Rotation quaternion (x, y, z, w) */
		FQuat Rotation = FQuat::Identity;

		/** RGB color (for visualization/initialization) */
		FColor Color = FColor::White;

		FGaussianSplat()
		{
			SH_Rest.SetNum(45);
			for (float& V : SH_Rest) V = 0.0f;
		}

		/** Create from position and color */
		static FGaussianSplat FromPositionColor(const FVector& Pos, const FColor& Col);

		/** Create from position, color, and normal */
		static FGaussianSplat FromPositionColorNormal(const FVector& Pos, const FColor& Col, const FVector& Norm);

		/** Convert RGB to DC SH coefficients */
		static FVector ColorToSH_DC(const FColor& Color);

		/** Convert DC SH coefficients to RGB */
		static FColor SH_DCToColor(const FVector& SH);
	};

	/**
	 * Point cloud data for initialization PLY
	 * Simpler format than full gaussian splats
	 */
	struct UNREALTOGAUSSIAN_API FPointCloudPoint
	{
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		FColor Color = FColor::White;
	};

	/**
	 * PLY writer for 3D Gaussian Splatting formats
	 *
	 * Supports:
	 * - Input PLY (point cloud for initialization)
	 * - Output PLY (full gaussian splats after training)
	 * - SPZ compressed format (90% size reduction)
	 */
	class UNREALTOGAUSSIAN_API FPlyWriter
	{
	public:
		/**
		 * Write point cloud PLY for 3DGS training initialization
		 *
		 * @param FilePath Output file path
		 * @param Points Point cloud data
		 * @param bBinary Write binary PLY (smaller, faster)
		 * @return True if successful
		 */
		static bool WritePointCloud(
			const FString& FilePath,
			const TArray<FPointCloudPoint>& Points,
			bool bBinary = true
		);

		/**
		 * Write full gaussian splats PLY
		 *
		 * @param FilePath Output file path
		 * @param Splats Gaussian splat data
		 * @param bBinary Write binary PLY
		 * @return True if successful
		 */
		static bool WriteGaussianSplats(
			const FString& FilePath,
			const TArray<FGaussianSplat>& Splats,
			bool bBinary = true
		);

		/**
		 * Create point cloud from mesh vertices
		 *
		 * @param Vertices Vertex positions
		 * @param Normals Vertex normals
		 * @param Colors Vertex colors
		 * @return Point cloud for PLY export
		 */
		static TArray<FPointCloudPoint> CreatePointCloudFromMesh(
			const TArray<FVector>& Vertices,
			const TArray<FVector>& Normals,
			const TArray<FColor>& Colors
		);

		/**
		 * Create initial gaussian splats from point cloud
		 * Initializes splats with reasonable defaults for training
		 *
		 * @param Points Point cloud data
		 * @param InitialScale Initial gaussian scale (log-space)
		 * @return Array of initialized gaussian splats
		 */
		static TArray<FGaussianSplat> CreateSplatsFromPointCloud(
			const TArray<FPointCloudPoint>& Points,
			float InitialScale = -5.0f
		);

		/**
		 * Read PLY file (point cloud format)
		 *
		 * @param FilePath Input file path
		 * @param OutPoints Output point cloud
		 * @return True if successful
		 */
		static bool ReadPointCloud(
			const FString& FilePath,
			TArray<FPointCloudPoint>& OutPoints
		);

		/**
		 * Read gaussian splats PLY
		 *
		 * @param FilePath Input file path
		 * @param OutSplats Output splats
		 * @return True if successful
		 */
		static bool ReadGaussianSplats(
			const FString& FilePath,
			TArray<FGaussianSplat>& OutSplats
		);

		/**
		 * Get PLY file statistics
		 *
		 * @param FilePath PLY file path
		 * @param OutNumVertices Number of vertices/splats
		 * @param OutIsBinary Whether file is binary format
		 * @param OutIsGaussian Whether file contains gaussian splat data
		 * @return True if file is valid PLY
		 */
		static bool GetPlyInfo(
			const FString& FilePath,
			int32& OutNumVertices,
			bool& OutIsBinary,
			bool& OutIsGaussian
		);

		/**
		 * Estimate memory usage for gaussian splats
		 *
		 * @param NumSplats Number of splats
		 * @return Memory in bytes
		 */
		static int64 EstimateMemoryUsage(int32 NumSplats);

		/**
		 * Validate splats for training
		 *
		 * @param Splats Splats to validate
		 * @param OutWarnings Output warnings
		 * @return True if valid
		 */
		static bool ValidateSplats(
			const TArray<FGaussianSplat>& Splats,
			TArray<FString>& OutWarnings
		);

	private:
		// PLY format helpers
		static FString GeneratePointCloudHeader(int32 NumPoints, bool bBinary);
		static FString GenerateGaussianHeader(int32 NumSplats, bool bBinary);

		static bool WritePointCloudBinary(const FString& FilePath, const TArray<FPointCloudPoint>& Points);
		static bool WritePointCloudASCII(const FString& FilePath, const TArray<FPointCloudPoint>& Points);
		static bool WriteGaussianBinary(const FString& FilePath, const TArray<FGaussianSplat>& Splats);
		static bool WriteGaussianASCII(const FString& FilePath, const TArray<FGaussianSplat>& Splats);

		static bool ParsePlyHeader(const FString& Content, int32& OutNumVertices, bool& OutIsBinary, TArray<FString>& OutProperties);
	};
}
