// Copyright 2024 3DGS Research Team. All Rights Reserved.

#include "SCM/CaptureOrchestrator.h"
#include "FCM/CoordinateConverter.h"
#include "FCM/ColmapWriter.h"
#include "FCM/PlyWriter.h"
#include "DEM/DepthExtractor.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "TimerManager.h"

UCaptureOrchestrator::UCaptureOrchestrator()
	: CurrentState(ECaptureState::Idle)
	, CurrentViewpointIndex(0)
	, CaptureWorld(nullptr)
	, SceneCaptureComponent(nullptr)
	, DepthCaptureComponent(nullptr)
	, ColorRenderTarget(nullptr)
	, DepthRenderTarget(nullptr)
	, CaptureStartTime(0.0)
{
}

bool UCaptureOrchestrator::StartCapture(UWorld* World, const FCaptureConfig& Config)
{
	if (CurrentState != ECaptureState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("Capture already in progress"));
		return false;
	}

	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid world for capture"));
		return false;
	}

	// Validate configuration
	TArray<FString> Warnings;
	if (!ValidateConfig(Config, Warnings))
	{
		for (const FString& Warning : Warnings)
		{
			UE_LOG(LogTemp, Warning, TEXT("Config validation: %s"), *Warning);
		}
	}

	// Store configuration
	ActiveConfig = Config;
	CaptureWorld = World;
	Result = FCaptureResult();
	Result.OutputPath = Config.OutputDirectory;
	CurrentViewpointIndex = 0;
	CaptureStartTime = FPlatformTime::Seconds();

	// Generate viewpoints
	CurrentState = ECaptureState::Preparing;
	Viewpoints = UE5_3DGS::FCameraTrajectoryGenerator::GenerateViewpoints(Config.TrajectoryConfig);

	if (Viewpoints.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to generate viewpoints"));
		CurrentState = ECaptureState::Error;
		Result.Errors.Add(TEXT("Failed to generate camera viewpoints"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Generated %d viewpoints for capture"), Viewpoints.Num());

	// Calculate camera intrinsics
	CameraIntrinsics = UE5_3DGS::FCameraIntrinsicsComputer::ComputeFromFOV(
		Config.FieldOfView,
		Config.ImageWidth,
		Config.ImageHeight,
		EColmapCameraModel::PINHOLE
	);

	// Setup scene capture
	if (!SetupSceneCapture(World))
	{
		CurrentState = ECaptureState::Error;
		Result.Errors.Add(TEXT("Failed to setup scene capture components"));
		return false;
	}

	// Create output directories
	if (!UE5_3DGS::FColmapWriter::CreateDirectoryStructure(Config.OutputDirectory))
	{
		CurrentState = ECaptureState::Error;
		Result.Errors.Add(TEXT("Failed to create output directories"));
		Cleanup();
		return false;
	}

	// Start capture loop
	CurrentState = ECaptureState::Capturing;

	// Use timer for sequential capture with delay
	if (Config.CaptureDelay > 0)
	{
		World->GetTimerManager().SetTimer(
			CaptureTimerHandle,
			this,
			&UCaptureOrchestrator::ProcessNextFrame,
			Config.CaptureDelay,
			true
		);
	}
	else
	{
		// Immediate capture (may cause rendering issues)
		ProcessNextFrame();
	}

	return true;
}

void UCaptureOrchestrator::StopCapture()
{
	if (CurrentState == ECaptureState::Idle)
	{
		return;
	}

	if (CaptureWorld && CaptureTimerHandle.IsValid())
	{
		CaptureWorld->GetTimerManager().ClearTimer(CaptureTimerHandle);
	}

	Result.bSuccess = false;
	Result.Errors.Add(TEXT("Capture cancelled by user"));
	CurrentState = ECaptureState::Idle;

	Cleanup();

	OnCaptureComplete.Broadcast(false);
}

float UCaptureOrchestrator::GetCaptureProgress() const
{
	if (Viewpoints.Num() == 0)
	{
		return 0.0f;
	}
	return static_cast<float>(CurrentViewpointIndex) / Viewpoints.Num();
}

bool UCaptureOrchestrator::CaptureSingleFrame(
	UWorld* World,
	const FTransform& CameraTransform,
	const FCaptureConfig& Config,
	FString& OutImagePath)
{
	if (!World)
	{
		return false;
	}

	// Temporarily setup capture
	FCaptureConfig TempConfig = Config;
	CaptureWorld = World;
	ActiveConfig = TempConfig;

	if (!SetupSceneCapture(World))
	{
		Cleanup();
		return false;
	}

	// Position capture component
	SceneCaptureComponent->SetWorldTransform(CameraTransform);

	// Capture
	SceneCaptureComponent->CaptureScene();

	// Read pixels
	TArray<FColor> Pixels;
	FTextureRenderTargetResource* RTResource = ColorRenderTarget->GameThread_GetRenderTargetResource();
	if (RTResource)
	{
		RTResource->ReadPixels(Pixels);
	}

	// Generate output path
	OutImagePath = Config.OutputDirectory / TEXT("preview.jpg");

	// Save image
	bool bSuccess = SaveImage(Pixels, Config.ImageWidth, Config.ImageHeight, OutImagePath);

	Cleanup();
	return bSuccess;
}

TArray<FTransform> UCaptureOrchestrator::PreviewTrajectory(const FTrajectoryConfig& Config)
{
	TArray<FTransform> Transforms;
	TArray<FCameraViewpoint> PreviewViewpoints = UE5_3DGS::FCameraTrajectoryGenerator::GenerateViewpoints(Config);

	Transforms.Reserve(PreviewViewpoints.Num());
	for (const FCameraViewpoint& VP : PreviewViewpoints)
	{
		Transforms.Add(VP.GetTransform());
	}

	return Transforms;
}

bool UCaptureOrchestrator::ValidateConfig(const FCaptureConfig& Config, TArray<FString>& OutWarnings)
{
	OutWarnings.Empty();
	bool bIsValid = true;

	// Check output directory
	if (Config.OutputDirectory.IsEmpty())
	{
		OutWarnings.Add(TEXT("Output directory not specified"));
		bIsValid = false;
	}

	// Check resolution
	if (Config.ImageWidth < 640 || Config.ImageHeight < 480)
	{
		OutWarnings.Add(TEXT("Resolution below 640x480 may result in poor training quality"));
	}

	if (Config.ImageWidth > 4096 || Config.ImageHeight > 4096)
	{
		OutWarnings.Add(TEXT("Resolution above 4096 may significantly increase capture and training time"));
	}

	// Validate trajectory
	TArray<FString> TrajectoryWarnings;
	UE5_3DGS::FCameraTrajectoryGenerator::ValidateConfig(Config.TrajectoryConfig, TrajectoryWarnings);
	OutWarnings.Append(TrajectoryWarnings);

	// Check FOV
	if (Config.FieldOfView < 45.0f || Config.FieldOfView > 120.0f)
	{
		OutWarnings.Add(FString::Printf(TEXT("Unusual FOV (%.1f). 60-90 recommended for 3DGS."), Config.FieldOfView));
	}

	return bIsValid;
}

void UCaptureOrchestrator::ProcessNextFrame()
{
	if (CurrentState != ECaptureState::Capturing)
	{
		return;
	}

	if (CurrentViewpointIndex >= Viewpoints.Num())
	{
		// All frames captured, move to export
		if (CaptureWorld && CaptureTimerHandle.IsValid())
		{
			CaptureWorld->GetTimerManager().ClearTimer(CaptureTimerHandle);
		}

		CurrentState = ECaptureState::Exporting;
		Result.TotalCaptureTime = FPlatformTime::Seconds() - CaptureStartTime;

		// Export COLMAP data
		if (!ExportColmapData())
		{
			Result.Warnings.Add(TEXT("Failed to export some COLMAP data"));
		}

		// Export point cloud if enabled
		if (ActiveConfig.bExportPointCloud)
		{
			if (!ExportPointCloud())
			{
				Result.Warnings.Add(TEXT("Failed to export point cloud"));
			}
		}

		// Complete
		CurrentState = ECaptureState::Complete;
		Result.bSuccess = Result.Errors.Num() == 0;

		Cleanup();
		OnCaptureComplete.Broadcast(Result.bSuccess);
		return;
	}

	const FCameraViewpoint& VP = Viewpoints[CurrentViewpointIndex];

	// Position capture components
	FTransform CameraTransform = VP.GetTransform();
	SceneCaptureComponent->SetWorldTransform(CameraTransform);

	if (DepthCaptureComponent)
	{
		DepthCaptureComponent->SetWorldTransform(CameraTransform);
	}

	// Capture color
	SceneCaptureComponent->CaptureScene();

	// Read and save color image
	TArray<FColor> ColorPixels;
	FTextureRenderTargetResource* ColorResource = ColorRenderTarget->GameThread_GetRenderTargetResource();
	if (ColorResource)
	{
		ColorResource->ReadPixels(ColorPixels);

		FString ImageFilename = FString::Printf(TEXT("image_%05d"), CurrentViewpointIndex);
		FString Extension = (ActiveConfig.ImageFormat == EImageFormat::JPEG) ? TEXT(".jpg") : TEXT(".png");
		FString ImagePath = ActiveConfig.OutputDirectory / TEXT("images") / (ImageFilename + Extension);

		if (SaveImage(ColorPixels, ActiveConfig.ImageWidth, ActiveConfig.ImageHeight, ImagePath))
		{
			Result.FramesCaptured++;
		}
		else
		{
			OnCaptureError.Broadcast(CurrentViewpointIndex, TEXT("Failed to save color image"));
		}
	}

	// Capture depth if enabled
	if (ActiveConfig.bCaptureDepth && DepthCaptureComponent)
	{
		DepthCaptureComponent->CaptureScene();

		UE5_3DGS::FDepthExtractionResult DepthResult = UE5_3DGS::FDepthExtractor::ExtractDepthFromRenderTarget(
			DepthRenderTarget,
			ActiveConfig.DepthConfig
		);

		if (DepthResult.IsValid())
		{
			FString DepthFilename = FString::Printf(TEXT("depth_%05d"), CurrentViewpointIndex);
			FString DepthPath = ActiveConfig.OutputDirectory / TEXT("depth") / DepthFilename;

			if (UE5_3DGS::FDepthExtractor::SaveDepthToFile(DepthResult, DepthPath, ActiveConfig.DepthConfig))
			{
				Result.DepthMapsCaptured++;
			}
		}
	}

	// Progress callback
	float Progress = static_cast<float>(CurrentViewpointIndex + 1) / Viewpoints.Num();
	OnCaptureProgress.Broadcast(CurrentViewpointIndex + 1, Viewpoints.Num(), Progress * 100.0f);

	CurrentViewpointIndex++;

	// Continue capture if not using timer
	if (ActiveConfig.CaptureDelay <= 0 && CurrentViewpointIndex < Viewpoints.Num())
	{
		ProcessNextFrame();
	}
}

bool UCaptureOrchestrator::ExportColmapData()
{
	// Create camera
	UE5_3DGS::FColmapCamera Camera;
	Camera.CameraId = 1;
	Camera.Intrinsics = CameraIntrinsics;
	Camera.bIsShared = true;

	TArray<UE5_3DGS::FColmapCamera> Cameras;
	Cameras.Add(Camera);

	// Create images from viewpoints
	FString Extension = (ActiveConfig.ImageFormat == EImageFormat::JPEG) ? TEXT(".jpg") : TEXT(".png");
	TArray<UE5_3DGS::FColmapImage> Images = UE5_3DGS::FColmapWriter::CreateImagesFromViewpoints(
		Viewpoints,
		CameraIntrinsics,
		TEXT("image_"),
		Extension
	);

	// Empty points3D (will be populated by COLMAP/training)
	TArray<UE5_3DGS::FColmapPoint3D> Points3D;

	return UE5_3DGS::FColmapWriter::WriteColmapDataset(
		ActiveConfig.OutputDirectory,
		Cameras,
		Images,
		Points3D,
		ActiveConfig.bUseBinaryColmap
	);
}

bool UCaptureOrchestrator::ExportPointCloud()
{
	// Generate initial point cloud from viewpoints
	// This creates a sparse point cloud based on camera positions
	// Real point cloud would come from depth fusion

	TArray<UE5_3DGS::FPointCloudPoint> Points;

	// For each viewpoint, add a point at the focus location
	// This is a placeholder - real implementation would use depth maps

	for (const FCameraViewpoint& VP : Viewpoints)
	{
		// Add camera position as point (for visualization)
		UE5_3DGS::FPointCloudPoint CamPoint;
		CamPoint.Position = UE5_3DGS::FCoordinateConverter::ConvertPositionToColmap(VP.Position);
		CamPoint.Normal = UE5_3DGS::FCoordinateConverter::ConvertDirectionToColmap(VP.Rotation.Vector());
		CamPoint.Color = FColor::Red;
		Points.Add(CamPoint);

		// Add focus point
		UE5_3DGS::FPointCloudPoint FocusPoint;
		FocusPoint.Position = UE5_3DGS::FCoordinateConverter::ConvertPositionToColmap(ActiveConfig.TrajectoryConfig.FocusPoint);
		FocusPoint.Normal = FVector::UpVector;
		FocusPoint.Color = FColor::White;
		Points.Add(FocusPoint);
	}

	// Write PLY
	FString PlyPath = ActiveConfig.OutputDirectory / TEXT("sparse") / TEXT("0") / TEXT("points3D.ply");
	return UE5_3DGS::FPlyWriter::WritePointCloud(PlyPath, Points, true);
}

bool UCaptureOrchestrator::SetupSceneCapture(UWorld* World)
{
	// Create actor to hold capture components
	AActor* CaptureActor = World->SpawnActor<AActor>();
	if (!CaptureActor)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create capture actor"));
		return false;
	}

	// Create color capture component
	SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(CaptureActor);
	if (!SceneCaptureComponent)
	{
		return false;
	}

	SceneCaptureComponent->RegisterComponent();
	SceneCaptureComponent->AttachToComponent(CaptureActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	// Create render target for color
	ColorRenderTarget = NewObject<UTextureRenderTarget2D>(World);
	ColorRenderTarget->RenderTargetFormat = RTF_RGBA8;
	ColorRenderTarget->ClearColor = FLinearColor::Black;
	ColorRenderTarget->bAutoGenerateMips = false;
	ColorRenderTarget->InitAutoFormat(ActiveConfig.ImageWidth, ActiveConfig.ImageHeight);
	ColorRenderTarget->UpdateResourceImmediate(true);

	SceneCaptureComponent->TextureTarget = ColorRenderTarget;
	SceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCaptureComponent->FOVAngle = ActiveConfig.FieldOfView;
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;

	// Configure capture settings
	if (ActiveConfig.bHideEditorElements)
	{
		SceneCaptureComponent->ShowFlags.SetEditor(false);
		SceneCaptureComponent->ShowFlags.SetGame(true);
	}

	if (ActiveConfig.bDisablePostProcessing)
	{
		SceneCaptureComponent->ShowFlags.SetPostProcessing(false);
		SceneCaptureComponent->ShowFlags.SetBloom(false);
		SceneCaptureComponent->ShowFlags.SetMotionBlur(false);
		SceneCaptureComponent->ShowFlags.SetToneCurve(false);
	}

	// Setup depth capture if enabled
	if (ActiveConfig.bCaptureDepth)
	{
		DepthCaptureComponent = NewObject<USceneCaptureComponent2D>(CaptureActor);
		if (DepthCaptureComponent)
		{
			DepthCaptureComponent->RegisterComponent();
			DepthCaptureComponent->AttachToComponent(CaptureActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

			DepthRenderTarget = UE5_3DGS::FDepthExtractor::CreateDepthRenderTarget(
				World,
				ActiveConfig.ImageWidth,
				ActiveConfig.ImageHeight
			);

			DepthCaptureComponent->TextureTarget = DepthRenderTarget;
			DepthCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
			DepthCaptureComponent->FOVAngle = ActiveConfig.FieldOfView;
			DepthCaptureComponent->bCaptureEveryFrame = false;
			DepthCaptureComponent->bCaptureOnMovement = false;
		}
	}

	return true;
}

void UCaptureOrchestrator::Cleanup()
{
	if (SceneCaptureComponent)
	{
		if (AActor* Owner = SceneCaptureComponent->GetOwner())
		{
			Owner->Destroy();
		}
		SceneCaptureComponent = nullptr;
		DepthCaptureComponent = nullptr;
	}

	ColorRenderTarget = nullptr;
	DepthRenderTarget = nullptr;
	CaptureWorld = nullptr;
}

bool UCaptureOrchestrator::SaveImage(const TArray<FColor>& Pixels, int32 Width, int32 Height, const FString& FilePath)
{
	if (Pixels.Num() != Width * Height)
	{
		return false;
	}

	TArray64<uint8> CompressedData;

	if (ActiveConfig.ImageFormat == EImageFormat::JPEG)
	{
		// JPEG compression
		FImageUtils::CompressImageArray(Width, Height, Pixels, CompressedData);
	}
	else
	{
		// PNG compression
		FImageUtils::PNGCompressImageArray(Width, Height, Pixels, CompressedData);
	}

	return FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
}
