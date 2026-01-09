// Copyright 2024 3DGS Research Team. All Rights Reserved.

#include "DEM/DepthExtractor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "RenderingThread.h"

namespace UE5_3DGS
{
	UTextureRenderTarget2D* FDepthExtractor::CreateDepthRenderTarget(
		UWorld* World,
		int32 Width,
		int32 Height)
	{
		UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(World);

		// Use R32F for high-precision depth
		RenderTarget->RenderTargetFormat = RTF_R32f;
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->bGPUSharedFlag = true;
		RenderTarget->InitAutoFormat(Width, Height);
		RenderTarget->UpdateResourceImmediate(true);

		return RenderTarget;
	}

	FDepthExtractionResult FDepthExtractor::ExtractDepthFromRenderTarget(
		UTextureRenderTarget2D* RenderTarget,
		const FDepthExtractionConfig& Config)
	{
		FDepthExtractionResult Result;

		if (!RenderTarget)
		{
			UE_LOG(LogTemp, Error, TEXT("Null render target for depth extraction"));
			return Result;
		}

		Result.Width = RenderTarget->SizeX;
		Result.Height = RenderTarget->SizeY;
		Result.NearPlane = Config.NearPlane;
		Result.FarPlane = Config.FarPlane;
		Result.bIsLinear = true;

		// Read render target data
		FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
		if (!RTResource)
		{
			UE_LOG(LogTemp, Error, TEXT("Could not get render target resource"));
			return Result;
		}

		// Read pixels based on format
		TArray<FFloat16Color> Float16Data;
		TArray<FLinearColor> LinearData;

		EPixelFormat PixelFormat = RenderTarget->GetFormat();

		if (PixelFormat == PF_R32_FLOAT || PixelFormat == PF_FloatRGBA)
		{
			// Read as linear color
			FReadSurfaceDataFlags ReadFlags(RCM_MinMax, CubeFace_MAX);
			RTResource->ReadLinearColorPixels(LinearData, ReadFlags);

			Result.DepthData.SetNum(Result.Width * Result.Height);
			Result.MinDepth = TNumericLimits<float>::Max();
			Result.MaxDepth = TNumericLimits<float>::Lowest();

			for (int32 i = 0; i < LinearData.Num(); ++i)
			{
				// Scene depth is in the R channel
				float SceneDepth = LinearData[i].R;

				// Convert from normalized scene depth to linear world units
				float LinearDepth = SceneDepthToLinear(SceneDepth, Config.NearPlane, Config.FarPlane);

				// Clamp to valid range
				LinearDepth = FMath::Clamp(LinearDepth, Config.NearPlane, Config.FarPlane);

				Result.DepthData[i] = LinearDepth;
				Result.MinDepth = FMath::Min(Result.MinDepth, LinearDepth);
				Result.MaxDepth = FMath::Max(Result.MaxDepth, LinearDepth);
			}
		}
		else
		{
			// Fallback: read as color and convert
			TArray<FColor> ColorData;
			RTResource->ReadPixels(ColorData);

			Result.DepthData.SetNum(Result.Width * Result.Height);
			Result.MinDepth = TNumericLimits<float>::Max();
			Result.MaxDepth = TNumericLimits<float>::Lowest();

			for (int32 i = 0; i < ColorData.Num(); ++i)
			{
				// Reconstruct depth from RGBA (assuming normalized encoding)
				float SceneDepth = ColorData[i].R / 255.0f;
				float LinearDepth = SceneDepthToLinear(SceneDepth, Config.NearPlane, Config.FarPlane);

				Result.DepthData[i] = LinearDepth;
				Result.MinDepth = FMath::Min(Result.MinDepth, LinearDepth);
				Result.MaxDepth = FMath::Max(Result.MaxDepth, LinearDepth);
			}
		}

		// Convert to meters if requested
		if (Config.bExportInMeters)
		{
			Result.ConvertToMeters();
		}

		// Apply gamma correction if requested
		if (Config.bApplyGammaCorrection)
		{
			float DepthRange = Result.MaxDepth - Result.MinDepth;
			if (DepthRange > 0)
			{
				for (float& D : Result.DepthData)
				{
					float Normalized = (D - Result.MinDepth) / DepthRange;
					Normalized = FMath::Pow(Normalized, 1.0f / Config.GammaValue);
					D = Result.MinDepth + Normalized * DepthRange;
				}
			}
		}

		// Invert depth if requested
		if (Config.bInvertDepth)
		{
			for (float& D : Result.DepthData)
			{
				if (D > 0.0001f)
				{
					D = 1.0f / D;
				}
			}
			// Swap min/max
			float Temp = Result.MinDepth;
			Result.MinDepth = 1.0f / Result.MaxDepth;
			Result.MaxDepth = 1.0f / Temp;
		}

		return Result;
	}

	float FDepthExtractor::SceneDepthToLinear(float SceneDepth, float NearPlane, float FarPlane)
	{
		// UE5 uses reversed-Z depth buffer
		// SceneDepth = 0 at far plane, SceneDepth = 1 at near plane

		if (SceneDepth >= 1.0f)
		{
			return NearPlane;
		}
		if (SceneDepth <= 0.0f)
		{
			return FarPlane;
		}

		// Reversed-Z: Z_buffer = (Far * Near) / (Far - Z * (Far - Near))
		// Solving for Z: Z = (Far * Near) / (Far - Z_buffer * (Far - Near))
		// With reversed-Z: Z_buffer = Near / Z, so Z = Near / Z_buffer

		// For UE5's specific depth encoding:
		float LinearDepth = NearPlane / SceneDepth;

		return FMath::Clamp(LinearDepth, NearPlane, FarPlane);
	}

	bool FDepthExtractor::SaveDepthToFile(
		const FDepthExtractionResult& Result,
		const FString& FilePath,
		const FDepthExtractionConfig& Config)
	{
		if (!Result.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid depth result, cannot save"));
			return false;
		}

		switch (Config.Format)
		{
		case EDepthFormat::PNG16:
			return SaveDepthAsPNG16(Result, FilePath);

		case EDepthFormat::EXR32:
			return SaveDepthAsEXR(Result, FilePath);

		case EDepthFormat::NPY:
			return SaveDepthAsNPY(Result, FilePath);

		case EDepthFormat::RawFloat32:
			return SaveDepthAsRawFloat(Result, FilePath);

		default:
			return SaveDepthAsEXR(Result, FilePath);
		}
	}

	bool FDepthExtractor::SaveDepthAsPNG16(
		const FDepthExtractionResult& Result,
		const FString& FilePath)
	{
		// Convert to 16-bit grayscale
		TArray<uint16> Pixels16;
		Pixels16.SetNum(Result.Width * Result.Height);

		float DepthRange = Result.MaxDepth - Result.MinDepth;
		if (DepthRange <= 0)
		{
			DepthRange = 1.0f;
		}

		for (int32 i = 0; i < Result.DepthData.Num(); ++i)
		{
			float Normalized = (Result.DepthData[i] - Result.MinDepth) / DepthRange;
			Normalized = FMath::Clamp(Normalized, 0.0f, 1.0f);
			Pixels16[i] = static_cast<uint16>(Normalized * 65535.0f);
		}

		// Convert to 8-bit RGBA for FImageUtils (PNG16 requires custom encoder)
		// For now, save as high-contrast 8-bit PNG
		TArray<FColor> Pixels;
		Pixels.SetNum(Result.Width * Result.Height);

		for (int32 i = 0; i < Pixels16.Num(); ++i)
		{
			uint8 Value = static_cast<uint8>(Pixels16[i] >> 8);
			Pixels[i] = FColor(Value, Value, Value, 255);
		}

		// Use FImageUtils to save PNG
		TArray64<uint8> CompressedData;
		FImageUtils::PNGCompressImageArray(Result.Width, Result.Height, Pixels, CompressedData);

		return FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
	}

	bool FDepthExtractor::SaveDepthAsEXR(
		const FDepthExtractionResult& Result,
		const FString& FilePath)
	{
		// Convert to FFloat16Color for EXR
		TArray<FFloat16Color> Float16Data;
		Float16Data.SetNum(Result.Width * Result.Height);

		for (int32 i = 0; i < Result.DepthData.Num(); ++i)
		{
			// Store depth in R channel
			Float16Data[i].R = Result.DepthData[i];
			Float16Data[i].G = 0.0f;
			Float16Data[i].B = 0.0f;
			Float16Data[i].A = 1.0f;
		}

		// EXR export requires ImageWriteQueue or custom encoder
		// For now, save as raw binary with .exr extension note
		TArray<uint8> RawData;
		RawData.SetNumUninitialized(Result.DepthData.Num() * sizeof(float));
		FMemory::Memcpy(RawData.GetData(), Result.DepthData.GetData(), RawData.Num());

		FString ActualPath = FPaths::ChangeExtension(FilePath, TEXT(".depth.raw"));
		bool bSuccess = FFileHelper::SaveArrayToFile(RawData, *ActualPath);

		// Save metadata alongside
		FString MetadataPath = FPaths::ChangeExtension(FilePath, TEXT(".depth.json"));
		FString Metadata = FString::Printf(
			TEXT("{\"width\":%d,\"height\":%d,\"min_depth\":%.6f,\"max_depth\":%.6f,\"format\":\"float32\",\"units\":\"meters\"}"),
			Result.Width, Result.Height, Result.MinDepth, Result.MaxDepth
		);
		FFileHelper::SaveStringToFile(Metadata, *MetadataPath);

		return bSuccess;
	}

	bool FDepthExtractor::SaveDepthAsNPY(
		const FDepthExtractionResult& Result,
		const FString& FilePath)
	{
		TArray<uint8> Header = CreateNPYHeader(Result.Width, Result.Height);

		// Combine header and data
		TArray<uint8> FileData;
		FileData.Append(Header);

		int32 DataSize = Result.DepthData.Num() * sizeof(float);
		int32 OldSize = FileData.Num();
		FileData.SetNumUninitialized(OldSize + DataSize);
		FMemory::Memcpy(FileData.GetData() + OldSize, Result.DepthData.GetData(), DataSize);

		return FFileHelper::SaveArrayToFile(FileData, *FilePath);
	}

	bool FDepthExtractor::SaveDepthAsRawFloat(
		const FDepthExtractionResult& Result,
		const FString& FilePath)
	{
		TArray<uint8> RawData;
		int32 DataSize = Result.DepthData.Num() * sizeof(float);
		RawData.SetNumUninitialized(DataSize);
		FMemory::Memcpy(RawData.GetData(), Result.DepthData.GetData(), DataSize);

		return FFileHelper::SaveArrayToFile(RawData, *FilePath);
	}

	TArray<FColor> FDepthExtractor::GenerateDepthVisualization(
		const FDepthExtractionResult& Result,
		bool bColorize)
	{
		TArray<FColor> Visualization;
		Visualization.SetNum(Result.Width * Result.Height);

		float DepthRange = Result.MaxDepth - Result.MinDepth;
		if (DepthRange <= 0)
		{
			DepthRange = 1.0f;
		}

		for (int32 i = 0; i < Result.DepthData.Num(); ++i)
		{
			float Normalized = (Result.DepthData[i] - Result.MinDepth) / DepthRange;
			Normalized = FMath::Clamp(Normalized, 0.0f, 1.0f);

			if (bColorize)
			{
				Visualization[i] = TurboColormap(Normalized);
			}
			else
			{
				uint8 Gray = static_cast<uint8>(Normalized * 255.0f);
				Visualization[i] = FColor(Gray, Gray, Gray, 255);
			}
		}

		return Visualization;
	}

	bool FDepthExtractor::ValidateForTraining(
		const FDepthExtractionResult& Result,
		TArray<FString>& OutWarnings)
	{
		OutWarnings.Empty();
		bool bIsValid = true;

		if (!Result.IsValid())
		{
			OutWarnings.Add(TEXT("Invalid depth result dimensions or data"));
			return false;
		}

		// Check for invalid values
		int32 InvalidCount = 0;
		int32 InfCount = 0;
		int32 NaNCount = 0;

		for (const float& D : Result.DepthData)
		{
			if (FMath::IsNaN(D))
			{
				NaNCount++;
			}
			else if (!FMath::IsFinite(D))
			{
				InfCount++;
			}
			else if (D <= 0)
			{
				InvalidCount++;
			}
		}

		if (NaNCount > 0)
		{
			OutWarnings.Add(FString::Printf(TEXT("%d NaN values detected in depth data"), NaNCount));
			bIsValid = false;
		}

		if (InfCount > 0)
		{
			OutWarnings.Add(FString::Printf(TEXT("%d infinite values detected in depth data"), InfCount));
		}

		float InvalidPercent = 100.0f * InvalidCount / Result.DepthData.Num();
		if (InvalidPercent > 5.0f)
		{
			OutWarnings.Add(FString::Printf(TEXT("%.1f%% invalid depth values (<=0)"), InvalidPercent));
		}

		// Check depth range
		float DepthRange = Result.MaxDepth - Result.MinDepth;
		if (DepthRange < 0.1f)
		{
			OutWarnings.Add(TEXT("Very narrow depth range (<0.1m). Scene may be flat."));
		}

		if (Result.MaxDepth > 1000.0f)
		{
			OutWarnings.Add(TEXT("Very large maximum depth (>1km). May affect precision."));
		}

		return bIsValid;
	}

	TArray<uint8> FDepthExtractor::CreateNPYHeader(int32 Width, int32 Height)
	{
		// NPY format: magic + version + header_len + header
		TArray<uint8> Header;

		// Magic number: \x93NUMPY
		Header.Add(0x93);
		Header.Add('N');
		Header.Add('U');
		Header.Add('M');
		Header.Add('P');
		Header.Add('Y');

		// Version 1.0
		Header.Add(0x01);
		Header.Add(0x00);

		// Header dict (describes array shape and dtype)
		FString HeaderDict = FString::Printf(
			TEXT("{'descr': '<f4', 'fortran_order': False, 'shape': (%d, %d), }"),
			Height, Width
		);

		// Pad to multiple of 64 bytes (including header length field)
		int32 CurrentLen = 10 + HeaderDict.Len(); // magic(6) + version(2) + header_len(2) + dict
		int32 PadLen = 64 - (CurrentLen % 64);
		if (PadLen < 64)
		{
			for (int32 i = 0; i < PadLen - 1; ++i)
			{
				HeaderDict += TEXT(" ");
			}
			HeaderDict += TEXT("\n");
		}

		// Header length (little-endian uint16)
		uint16 HeaderLen = static_cast<uint16>(HeaderDict.Len());
		Header.Add(HeaderLen & 0xFF);
		Header.Add((HeaderLen >> 8) & 0xFF);

		// Header dict as ASCII
		for (TCHAR C : HeaderDict)
		{
			Header.Add(static_cast<uint8>(C));
		}

		return Header;
	}

	FColor FDepthExtractor::TurboColormap(float NormalizedValue)
	{
		// Turbo colormap approximation
		NormalizedValue = FMath::Clamp(NormalizedValue, 0.0f, 1.0f);

		float R, G, B;

		if (NormalizedValue < 0.25f)
		{
			float T = NormalizedValue / 0.25f;
			R = 0.18995f + T * (0.35238f - 0.18995f);
			G = 0.07176f + T * (0.34290f - 0.07176f);
			B = 0.23217f + T * (0.93411f - 0.23217f);
		}
		else if (NormalizedValue < 0.5f)
		{
			float T = (NormalizedValue - 0.25f) / 0.25f;
			R = 0.35238f + T * (0.56924f - 0.35238f);
			G = 0.34290f + T * (0.77063f - 0.34290f);
			B = 0.93411f + T * (0.46915f - 0.93411f);
		}
		else if (NormalizedValue < 0.75f)
		{
			float T = (NormalizedValue - 0.5f) / 0.25f;
			R = 0.56924f + T * (0.94227f - 0.56924f);
			G = 0.77063f + T * (0.89411f - 0.77063f);
			B = 0.46915f + T * (0.10175f - 0.46915f);
		}
		else
		{
			float T = (NormalizedValue - 0.75f) / 0.25f;
			R = 0.94227f + T * (0.98644f - 0.94227f);
			G = 0.89411f + T * (0.46916f - 0.89411f);
			B = 0.10175f + T * (0.07991f - 0.10175f);
		}

		return FColor(
			static_cast<uint8>(R * 255.0f),
			static_cast<uint8>(G * 255.0f),
			static_cast<uint8>(B * 255.0f),
			255
		);
	}
}
