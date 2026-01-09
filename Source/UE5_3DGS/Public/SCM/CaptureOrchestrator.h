// Copyright 2024 3DGS Research Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SCM/CameraTrajectory.h"
#include "FCM/CameraIntrinsics.h"
#include "DEM/DepthExtractor.h"
#include "CaptureOrchestrator.generated.h"

/**
 * Capture progress delegate
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCaptureProgress, int32, CurrentFrame, int32, TotalFrames, float, ProgressPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCaptureComplete, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCaptureError, int32, FrameIndex, const FString&, ErrorMessage);

/**
 * Capture configuration
 */
USTRUCT(BlueprintType)
struct UNREALTOGAUSSIAN_API FCaptureConfig
{
	GENERATED_BODY()

	/** Output directory for captured data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	FString OutputDirectory;

	/** Image width */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "256", ClampMax = "8192"))
	int32 ImageWidth = 1920;

	/** Image height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "256", ClampMax = "8192"))
	int32 ImageHeight = 1080;

	/** Camera field of view (horizontal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "30.0", ClampMax = "170.0"))
	float FieldOfView = 90.0f;

	/** Trajectory configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	FTrajectoryConfig TrajectoryConfig;

	/** Whether to capture depth maps */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	bool bCaptureDepth = true;

	/** Depth extraction configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	FDepthExtractionConfig DepthConfig;

	/** Whether to export point cloud */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	bool bExportPointCloud = true;

	/** Image format */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	EImageFormat ImageFormat = EImageFormat::JPEG;

	/** JPEG quality (0-100) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "1", ClampMax = "100"))
	int32 JpegQuality = 95;

	/** Whether to use binary COLMAP format */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	bool bUseBinaryColmap = false;

	/** Delay between captures (seconds) for scene settling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "0.0"))
	float CaptureDelay = 0.1f;

	/** Whether to hide player/editor elements during capture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	bool bHideEditorElements = true;

	/** Whether to disable post-processing during capture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	bool bDisablePostProcessing = false;

	/** Antialiasing samples (1 = disabled, 4/8 = MSAA) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "1", ClampMax = "8"))
	int32 AntialiasSamples = 1;
};

/**
 * Capture state tracking
 */
UENUM(BlueprintType)
enum class ECaptureState : uint8
{
	Idle,
	Preparing,
	Capturing,
	Processing,
	Exporting,
	Complete,
	Error
};

/**
 * Capture result data
 */
USTRUCT(BlueprintType)
struct UNREALTOGAUSSIAN_API FCaptureResult
{
	GENERATED_BODY()

	/** Whether capture was successful */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	/** Number of frames captured */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 FramesCaptured = 0;

	/** Number of depth maps captured */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 DepthMapsCaptured = 0;

	/** Output directory path */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString OutputPath;

	/** Total capture time in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float TotalCaptureTime = 0.0f;

	/** Error messages if any */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FString> Errors;

	/** Warnings */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FString> Warnings;
};

/**
 * Main capture orchestration class for 3DGS dataset generation
 *
 * Coordinates:
 * - Camera trajectory generation
 * - Scene capture at each viewpoint
 * - Depth buffer extraction
 * - COLMAP format export
 * - Point cloud generation
 */
UCLASS(BlueprintType, Blueprintable)
class UNREALTOGAUSSIAN_API UCaptureOrchestrator : public UObject
{
	GENERATED_BODY()

public:
	UCaptureOrchestrator();

	/**
	 * Start capture process
	 *
	 * @param World World to capture
	 * @param Config Capture configuration
	 * @return True if capture started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "3DGS Capture")
	bool StartCapture(UWorld* World, const FCaptureConfig& Config);

	/**
	 * Stop/cancel ongoing capture
	 */
	UFUNCTION(BlueprintCallable, Category = "3DGS Capture")
	void StopCapture();

	/**
	 * Get current capture state
	 */
	UFUNCTION(BlueprintCallable, Category = "3DGS Capture")
	ECaptureState GetCaptureState() const { return CurrentState; }

	/**
	 * Get capture progress (0-1)
	 */
	UFUNCTION(BlueprintCallable, Category = "3DGS Capture")
	float GetCaptureProgress() const;

	/**
	 * Get capture result (valid after Complete state)
	 */
	UFUNCTION(BlueprintCallable, Category = "3DGS Capture")
	FCaptureResult GetCaptureResult() const { return Result; }

	/**
	 * Single-frame capture (for preview/testing)
	 *
	 * @param World World to capture
	 * @param CameraTransform Camera transform for capture
	 * @param Config Capture configuration
	 * @param OutImagePath Output image path
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "3DGS Capture")
	bool CaptureSingleFrame(
		UWorld* World,
		const FTransform& CameraTransform,
		const FCaptureConfig& Config,
		FString& OutImagePath
	);

	/**
	 * Preview trajectory without capturing
	 *
	 * @param Config Capture configuration
	 * @return Array of viewpoint transforms
	 */
	UFUNCTION(BlueprintCallable, Category = "3DGS Capture")
	TArray<FTransform> PreviewTrajectory(const FTrajectoryConfig& Config);

	/**
	 * Validate configuration before capture
	 *
	 * @param Config Configuration to validate
	 * @param OutWarnings Output warnings
	 * @return True if valid
	 */
	UFUNCTION(BlueprintCallable, Category = "3DGS Capture")
	bool ValidateConfig(const FCaptureConfig& Config, TArray<FString>& OutWarnings);

	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "3DGS Capture")
	FOnCaptureProgress OnCaptureProgress;

	UPROPERTY(BlueprintAssignable, Category = "3DGS Capture")
	FOnCaptureComplete OnCaptureComplete;

	UPROPERTY(BlueprintAssignable, Category = "3DGS Capture")
	FOnCaptureError OnCaptureError;

protected:
	/** Process next frame in capture sequence */
	void ProcessNextFrame();

	/** Export COLMAP data after capture */
	bool ExportColmapData();

	/** Export point cloud after capture */
	bool ExportPointCloud();

	/** Setup scene capture component */
	bool SetupSceneCapture(UWorld* World);

	/** Cleanup after capture */
	void Cleanup();

	/** Save captured image to file */
	bool SaveImage(const TArray<FColor>& Pixels, int32 Width, int32 Height, const FString& FilePath);

private:
	UPROPERTY()
	ECaptureState CurrentState;

	UPROPERTY()
	FCaptureConfig ActiveConfig;

	UPROPERTY()
	FCaptureResult Result;

	UPROPERTY()
	TArray<FCameraViewpoint> Viewpoints;

	UPROPERTY()
	int32 CurrentViewpointIndex;

	UPROPERTY()
	UWorld* CaptureWorld;

	UPROPERTY()
	class USceneCaptureComponent2D* SceneCaptureComponent;

	UPROPERTY()
	class USceneCaptureComponent2D* DepthCaptureComponent;

	UPROPERTY()
	class UTextureRenderTarget2D* ColorRenderTarget;

	UPROPERTY()
	class UTextureRenderTarget2D* DepthRenderTarget;

	UPROPERTY()
	FCameraIntrinsics CameraIntrinsics;

	double CaptureStartTime;

	FTimerHandle CaptureTimerHandle;
};
