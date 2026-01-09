// Copyright 2024 3DGS Research Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "DepthExtractor.generated.h"

/**
 * Depth buffer format for export
 */
UENUM(BlueprintType)
enum class EDepthFormat : uint8
{
	/** 16-bit PNG (normalized 0-65535) */
	PNG16 UMETA(DisplayName = "PNG 16-bit"),

	/** 32-bit EXR (linear meters) */
	EXR32 UMETA(DisplayName = "EXR 32-bit"),

	/** NumPy NPY format (float32) */
	NPY UMETA(DisplayName = "NumPy NPY"),

	/** Raw binary float32 */
	RawFloat32 UMETA(DisplayName = "Raw Float32")
};

/**
 * Depth extraction result containing raw depth data and metadata
 */
USTRUCT(BlueprintType)
struct UNREALTOGAUSSIAN_API FDepthExtractionResult
{
	GENERATED_BODY()

	/** Raw depth values in centimeters (UE5 units) */
	TArray<float> DepthData;

	/** Image width */
	UPROPERTY(BlueprintReadOnly, Category = "Depth")
	int32 Width = 0;

	/** Image height */
	UPROPERTY(BlueprintReadOnly, Category = "Depth")
	int32 Height = 0;

	/** Minimum depth value captured */
	UPROPERTY(BlueprintReadOnly, Category = "Depth")
	float MinDepth = 0.0f;

	/** Maximum depth value captured */
	UPROPERTY(BlueprintReadOnly, Category = "Depth")
	float MaxDepth = 0.0f;

	/** Near clip plane used during capture */
	UPROPERTY(BlueprintReadOnly, Category = "Depth")
	float NearPlane = 10.0f;

	/** Far clip plane used during capture */
	UPROPERTY(BlueprintReadOnly, Category = "Depth")
	float FarPlane = 100000.0f;

	/** Whether depth is in linear or normalized format */
	UPROPERTY(BlueprintReadOnly, Category = "Depth")
	bool bIsLinear = true;

	/** Check if result is valid */
	bool IsValid() const
	{
		return Width > 0 && Height > 0 && DepthData.Num() == Width * Height;
	}

	/** Get depth at pixel coordinate */
	float GetDepthAt(int32 X, int32 Y) const
	{
		if (X >= 0 && X < Width && Y >= 0 && Y < Height)
		{
			return DepthData[Y * Width + X];
		}
		return -1.0f;
	}

	/** Convert depth from centimeters to meters */
	void ConvertToMeters()
	{
		for (float& D : DepthData)
		{
			D *= 0.01f;
		}
		MinDepth *= 0.01f;
		MaxDepth *= 0.01f;
		NearPlane *= 0.01f;
		FarPlane *= 0.01f;
	}
};

/**
 * Configuration for depth extraction
 */
USTRUCT(BlueprintType)
struct UNREALTOGAUSSIAN_API FDepthExtractionConfig
{
	GENERATED_BODY()

	/** Export format */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth")
	EDepthFormat Format = EDepthFormat::EXR32;

	/** Near clip plane in centimeters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth", meta = (ClampMin = "1.0"))
	float NearPlane = 10.0f;

	/** Far clip plane in centimeters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth", meta = (ClampMin = "100.0"))
	float FarPlane = 100000.0f;

	/** Whether to export depth in meters (COLMAP convention) or centimeters (UE5) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth")
	bool bExportInMeters = true;

	/** Whether to apply gamma correction (for visualization) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth")
	bool bApplyGammaCorrection = false;

	/** Gamma value for correction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float GammaValue = 2.2f;

	/** Whether to invert depth (1/z) for certain formats */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth")
	bool bInvertDepth = false;
};

namespace UE5_3DGS
{
	/**
	 * Depth buffer extraction utilities for 3DGS
	 *
	 * Extracts depth from SceneCapture components and converts to
	 * formats suitable for 3DGS training (COLMAP depth maps).
	 */
	class UNREALTOGAUSSIAN_API FDepthExtractor
	{
	public:
		/**
		 * Create a depth capture render target
		 *
		 * @param World World to create the render target in
		 * @param Width Target width
		 * @param Height Target height
		 * @return Configured render target for depth capture
		 */
		static UTextureRenderTarget2D* CreateDepthRenderTarget(
			UWorld* World,
			int32 Width,
			int32 Height
		);

		/**
		 * Extract depth data from a render target
		 *
		 * @param RenderTarget The depth render target to read from
		 * @param Config Extraction configuration
		 * @return Depth extraction result
		 */
		static FDepthExtractionResult ExtractDepthFromRenderTarget(
			UTextureRenderTarget2D* RenderTarget,
			const FDepthExtractionConfig& Config
		);

		/**
		 * Convert scene depth to linear depth
		 * UE5 uses a reversed-Z depth buffer with non-linear encoding
		 *
		 * @param SceneDepth Raw scene depth value (0-1)
		 * @param NearPlane Near clip plane
		 * @param FarPlane Far clip plane
		 * @return Linear depth in world units
		 */
		static float SceneDepthToLinear(float SceneDepth, float NearPlane, float FarPlane);

		/**
		 * Save depth data to file
		 *
		 * @param Result Depth extraction result
		 * @param FilePath Output file path
		 * @param Config Extraction configuration
		 * @return True if saved successfully
		 */
		static bool SaveDepthToFile(
			const FDepthExtractionResult& Result,
			const FString& FilePath,
			const FDepthExtractionConfig& Config
		);

		/**
		 * Save depth as 16-bit PNG
		 * Depth normalized to 0-65535 range based on near/far planes
		 *
		 * @param Result Depth data
		 * @param FilePath Output path
		 * @return True if successful
		 */
		static bool SaveDepthAsPNG16(
			const FDepthExtractionResult& Result,
			const FString& FilePath
		);

		/**
		 * Save depth as 32-bit EXR
		 * Linear depth in meters
		 *
		 * @param Result Depth data
		 * @param FilePath Output path
		 * @return True if successful
		 */
		static bool SaveDepthAsEXR(
			const FDepthExtractionResult& Result,
			const FString& FilePath
		);

		/**
		 * Save depth as NumPy NPY file
		 * Compatible with Python 3DGS training scripts
		 *
		 * @param Result Depth data
		 * @param FilePath Output path
		 * @return True if successful
		 */
		static bool SaveDepthAsNPY(
			const FDepthExtractionResult& Result,
			const FString& FilePath
		);

		/**
		 * Save depth as raw float32 binary
		 *
		 * @param Result Depth data
		 * @param FilePath Output path
		 * @return True if successful
		 */
		static bool SaveDepthAsRawFloat(
			const FDepthExtractionResult& Result,
			const FString& FilePath
		);

		/**
		 * Generate depth visualization (for debugging)
		 *
		 * @param Result Depth data
		 * @param bColorize Apply turbo/viridis colormap
		 * @return Color array for visualization
		 */
		static TArray<FColor> GenerateDepthVisualization(
			const FDepthExtractionResult& Result,
			bool bColorize = true
		);

		/**
		 * Validate depth data for 3DGS training
		 *
		 * @param Result Depth data to validate
		 * @param OutWarnings Output warnings
		 * @return True if valid for training
		 */
		static bool ValidateForTraining(
			const FDepthExtractionResult& Result,
			TArray<FString>& OutWarnings
		);

	private:
		/** Write NPY header for float32 array */
		static TArray<uint8> CreateNPYHeader(int32 Width, int32 Height);

		/** Apply turbo colormap to normalized depth value */
		static FColor TurboColormap(float NormalizedValue);
	};
}
