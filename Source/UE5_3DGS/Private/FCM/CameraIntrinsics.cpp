// Copyright 2024 3DGS Research Team. All Rights Reserved.

#include "FCM/CameraIntrinsics.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CineCameraComponent.h"

// FCameraIntrinsics implementation

FCameraIntrinsics::FCameraIntrinsics(int32 InWidth, int32 InHeight, float HorizontalFOVDegrees)
	: Width(InWidth)
	, Height(InHeight)
	, CameraModel(EColmapCameraModel::PINHOLE)
{
	FocalLengthX = ComputeFocalLengthFromFOV(HorizontalFOVDegrees, Width);
	FocalLengthY = FocalLengthX; // Square pixels
	PrincipalPointX = Width / 2.0;
	PrincipalPointY = Height / 2.0;
	K1 = K2 = P1 = P2 = 0.0;
}

double FCameraIntrinsics::ComputeFocalLengthFromFOV(double FOVDegrees, double ImageDimension)
{
	// f = (dimension / 2) / tan(FOV / 2)
	double FOVRadians = FMath::DegreesToRadians(FOVDegrees);
	return (ImageDimension / 2.0) / FMath::Tan(FOVRadians / 2.0);
}

double FCameraIntrinsics::ComputeFOVFromFocalLength(double FocalLength, double ImageDimension)
{
	// FOV = 2 * atan((dimension / 2) / f)
	return FMath::RadiansToDegrees(2.0 * FMath::Atan((ImageDimension / 2.0) / FocalLength));
}

bool FCameraIntrinsics::IsValid() const
{
	return Width > 0
		&& Height > 0
		&& FocalLengthX > 0
		&& FocalLengthY > 0
		&& PrincipalPointX > 0
		&& PrincipalPointY > 0;
}

FMatrix FCameraIntrinsics::GetIntrinsicMatrix() const
{
	// Intrinsic matrix K:
	// [fx  0  cx]
	// [0  fy  cy]
	// [0   0   1]
	FMatrix K = FMatrix::Identity;
	K.M[0][0] = FocalLengthX;
	K.M[1][1] = FocalLengthY;
	K.M[0][2] = PrincipalPointX;
	K.M[1][2] = PrincipalPointY;
	return K;
}

FString FCameraIntrinsics::GetColmapParamsString() const
{
	switch (CameraModel)
	{
	case EColmapCameraModel::SIMPLE_PINHOLE:
		// f, cx, cy
		return FString::Printf(TEXT("%.10f %.10f %.10f"),
			FocalLengthX, PrincipalPointX, PrincipalPointY);

	case EColmapCameraModel::PINHOLE:
		// fx, fy, cx, cy
		return FString::Printf(TEXT("%.10f %.10f %.10f %.10f"),
			FocalLengthX, FocalLengthY, PrincipalPointX, PrincipalPointY);

	case EColmapCameraModel::SIMPLE_RADIAL:
		// f, cx, cy, k1
		return FString::Printf(TEXT("%.10f %.10f %.10f %.10f"),
			FocalLengthX, PrincipalPointX, PrincipalPointY, K1);

	case EColmapCameraModel::RADIAL:
		// f, cx, cy, k1, k2
		return FString::Printf(TEXT("%.10f %.10f %.10f %.10f %.10f"),
			FocalLengthX, PrincipalPointX, PrincipalPointY, K1, K2);

	case EColmapCameraModel::OPENCV:
		// fx, fy, cx, cy, k1, k2, p1, p2
		return FString::Printf(TEXT("%.10f %.10f %.10f %.10f %.10f %.10f %.10f %.10f"),
			FocalLengthX, FocalLengthY, PrincipalPointX, PrincipalPointY,
			K1, K2, P1, P2);

	case EColmapCameraModel::FULL_OPENCV:
		// fx, fy, cx, cy, k1, k2, p1, p2, k3, k4, k5, k6 (we only have k1, k2)
		return FString::Printf(TEXT("%.10f %.10f %.10f %.10f %.10f %.10f %.10f %.10f 0.0 0.0 0.0 0.0"),
			FocalLengthX, FocalLengthY, PrincipalPointX, PrincipalPointY,
			K1, K2, P1, P2);

	default:
		return GetColmapParamsString();
	}
}

int32 FCameraIntrinsics::GetColmapModelId() const
{
	return static_cast<int32>(CameraModel);
}

FString FCameraIntrinsics::GetColmapModelName() const
{
	switch (CameraModel)
	{
	case EColmapCameraModel::SIMPLE_PINHOLE: return TEXT("SIMPLE_PINHOLE");
	case EColmapCameraModel::PINHOLE: return TEXT("PINHOLE");
	case EColmapCameraModel::SIMPLE_RADIAL: return TEXT("SIMPLE_RADIAL");
	case EColmapCameraModel::RADIAL: return TEXT("RADIAL");
	case EColmapCameraModel::OPENCV: return TEXT("OPENCV");
	case EColmapCameraModel::FULL_OPENCV: return TEXT("FULL_OPENCV");
	default: return TEXT("UNKNOWN");
	}
}

// FCameraIntrinsicsComputer implementation

namespace UE5_3DGS
{
	FCameraIntrinsics FCameraIntrinsicsComputer::ComputeFromCameraComponent(
		const UCameraComponent* CameraComponent,
		int32 ImageWidth,
		int32 ImageHeight)
	{
		FCameraIntrinsics Result;
		Result.Width = ImageWidth;
		Result.Height = ImageHeight;
		Result.CameraModel = EColmapCameraModel::PINHOLE;

		if (!CameraComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("Null camera component, using default intrinsics"));
			Result.FocalLengthX = FCameraIntrinsics::ComputeFocalLengthFromFOV(90.0f, ImageWidth);
			Result.FocalLengthY = Result.FocalLengthX;
			Result.PrincipalPointX = ImageWidth / 2.0;
			Result.PrincipalPointY = ImageHeight / 2.0;
			return Result;
		}

		// Check if it's a CineCameraComponent for more accurate settings
		const UCineCameraComponent* CineCamera = Cast<UCineCameraComponent>(CameraComponent);
		if (CineCamera)
		{
			// Use physical camera settings
			float SensorWidth = CineCamera->Filmback.SensorWidth;
			float SensorHeight = CineCamera->Filmback.SensorHeight;
			float FocalLengthMM = CineCamera->CurrentFocalLength;

			return ComputeFromSensorAndFocalLength(
				SensorWidth, SensorHeight, FocalLengthMM,
				ImageWidth, ImageHeight
			);
		}

		// Standard camera component - use FOV
		float HorizontalFOV = CameraComponent->FieldOfView;

		// UE5 uses horizontal FOV
		Result.FocalLengthX = FCameraIntrinsics::ComputeFocalLengthFromFOV(HorizontalFOV, ImageWidth);
		Result.FocalLengthY = Result.FocalLengthX; // Square pixels
		Result.PrincipalPointX = ImageWidth / 2.0;
		Result.PrincipalPointY = ImageHeight / 2.0;

		return Result;
	}

	FCameraIntrinsics FCameraIntrinsicsComputer::ComputeFromSceneCaptureComponent(
		const USceneCaptureComponent2D* CaptureComponent,
		const UTextureRenderTarget2D* RenderTarget)
	{
		FCameraIntrinsics Result;
		Result.CameraModel = EColmapCameraModel::PINHOLE;

		if (!RenderTarget)
		{
			UE_LOG(LogTemp, Warning, TEXT("Null render target"));
			return Result;
		}

		Result.Width = RenderTarget->SizeX;
		Result.Height = RenderTarget->SizeY;

		if (!CaptureComponent)
		{
			Result.FocalLengthX = FCameraIntrinsics::ComputeFocalLengthFromFOV(90.0f, Result.Width);
			Result.FocalLengthY = Result.FocalLengthX;
			Result.PrincipalPointX = Result.Width / 2.0;
			Result.PrincipalPointY = Result.Height / 2.0;
			return Result;
		}

		// SceneCaptureComponent2D uses FOVAngle property
		float FOV = CaptureComponent->FOVAngle;

		Result.FocalLengthX = FCameraIntrinsics::ComputeFocalLengthFromFOV(FOV, Result.Width);
		Result.FocalLengthY = Result.FocalLengthX;
		Result.PrincipalPointX = Result.Width / 2.0;
		Result.PrincipalPointY = Result.Height / 2.0;

		return Result;
	}

	FCameraIntrinsics FCameraIntrinsicsComputer::ComputeFromFOV(
		float HorizontalFOVDegrees,
		int32 ImageWidth,
		int32 ImageHeight,
		EColmapCameraModel CameraModel)
	{
		FCameraIntrinsics Result;
		Result.Width = ImageWidth;
		Result.Height = ImageHeight;
		Result.CameraModel = CameraModel;

		Result.FocalLengthX = FCameraIntrinsics::ComputeFocalLengthFromFOV(HorizontalFOVDegrees, ImageWidth);
		Result.FocalLengthY = Result.FocalLengthX;
		Result.PrincipalPointX = ImageWidth / 2.0;
		Result.PrincipalPointY = ImageHeight / 2.0;

		return Result;
	}

	FCameraIntrinsics FCameraIntrinsicsComputer::ComputeFromSensorAndFocalLength(
		float SensorWidthMM,
		float SensorHeightMM,
		float FocalLengthMM,
		int32 ImageWidth,
		int32 ImageHeight)
	{
		FCameraIntrinsics Result;
		Result.Width = ImageWidth;
		Result.Height = ImageHeight;
		Result.CameraModel = EColmapCameraModel::PINHOLE;

		// Convert physical focal length to pixel focal length
		// fx = (f_mm / sensor_width_mm) * image_width_pixels
		Result.FocalLengthX = (FocalLengthMM / SensorWidthMM) * ImageWidth;
		Result.FocalLengthY = (FocalLengthMM / SensorHeightMM) * ImageHeight;

		// Principal point at image center
		Result.PrincipalPointX = ImageWidth / 2.0;
		Result.PrincipalPointY = ImageHeight / 2.0;

		return Result;
	}

	bool FCameraIntrinsicsComputer::ValidateFor3DGS(
		const FCameraIntrinsics& Intrinsics,
		TArray<FString>& OutWarnings)
	{
		OutWarnings.Empty();
		bool bIsValid = true;

		// Check resolution
		if (Intrinsics.Width < 800 || Intrinsics.Height < 600)
		{
			OutWarnings.Add(TEXT("Resolution below 800x600 may result in poor 3DGS training quality"));
		}

		if (Intrinsics.Width > 4096 || Intrinsics.Height > 4096)
		{
			OutWarnings.Add(TEXT("Resolution above 4096 may significantly increase training time"));
		}

		// Check focal length
		double EstimatedHFOV = FCameraIntrinsics::ComputeFOVFromFocalLength(
			Intrinsics.FocalLengthX, Intrinsics.Width);

		if (EstimatedHFOV < 30.0)
		{
			OutWarnings.Add(FString::Printf(TEXT("Very narrow FOV (%.1f deg) may cause sparse coverage"), EstimatedHFOV));
		}
		else if (EstimatedHFOV > 120.0)
		{
			OutWarnings.Add(FString::Printf(TEXT("Very wide FOV (%.1f deg) may cause distortion issues"), EstimatedHFOV));
		}

		// Check aspect ratio
		double AspectRatio = Intrinsics.GetAspectRatio();
		if (AspectRatio < 0.5 || AspectRatio > 2.5)
		{
			OutWarnings.Add(FString::Printf(TEXT("Unusual aspect ratio (%.2f) - typical is 1.33-1.78"), AspectRatio));
		}

		// Check principal point
		double CxOffset = FMath::Abs(Intrinsics.PrincipalPointX - Intrinsics.Width / 2.0);
		double CyOffset = FMath::Abs(Intrinsics.PrincipalPointY - Intrinsics.Height / 2.0);

		if (CxOffset > Intrinsics.Width * 0.1 || CyOffset > Intrinsics.Height * 0.1)
		{
			OutWarnings.Add(TEXT("Principal point significantly off-center (>10%)"));
		}

		// Validity checks
		if (!Intrinsics.IsValid())
		{
			OutWarnings.Add(TEXT("Invalid intrinsics: zero or negative values detected"));
			bIsValid = false;
		}

		// Check for non-square pixels
		double FocalRatio = Intrinsics.FocalLengthX / Intrinsics.FocalLengthY;
		if (FMath::Abs(FocalRatio - 1.0) > 0.01)
		{
			OutWarnings.Add(FString::Printf(TEXT("Non-square pixels detected (fx/fy = %.3f)"), FocalRatio));
		}

		return bIsValid;
	}
}
