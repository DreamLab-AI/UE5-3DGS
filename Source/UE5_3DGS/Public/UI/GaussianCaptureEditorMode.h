// Copyright 2024 3DGS Research Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorModes.h"
#include "EdMode.h"
#include "SCM/CameraTrajectory.h"
#include "SCM/CaptureOrchestrator.h"

/**
 * Editor mode for 3D Gaussian Splatting capture setup and preview
 *
 * Features:
 * - Visual trajectory preview with camera frustums
 * - Interactive focus point adjustment
 * - Real-time parameter tuning
 * - One-click capture initiation
 */
class UNREALTOGAUSSIAN_API FGaussianCaptureEditorMode : public FEdMode
{
public:
	const static FEditorModeID EM_GaussianCapture;

	FGaussianCaptureEditorMode();
	virtual ~FGaussianCaptureEditorMode();

	// FEdMode interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	virtual bool UsesToolkits() const override { return true; }

	/** Get current capture configuration */
	const FCaptureConfig& GetCaptureConfig() const { return CaptureConfig; }

	/** Set capture configuration */
	void SetCaptureConfig(const FCaptureConfig& Config);

	/** Get trajectory configuration (convenience) */
	const FTrajectoryConfig& GetTrajectoryConfig() const { return CaptureConfig.TrajectoryConfig; }

	/** Set trajectory configuration */
	void SetTrajectoryConfig(const FTrajectoryConfig& Config);

	/** Get preview viewpoints */
	const TArray<FCameraViewpoint>& GetPreviewViewpoints() const { return PreviewViewpoints; }

	/** Regenerate preview trajectory */
	void RegenerateTrajectory();

	/** Start capture with current configuration */
	bool StartCapture();

	/** Stop ongoing capture */
	void StopCapture();

	/** Get capture progress (0-1) */
	float GetCaptureProgress() const;

	/** Check if capture is in progress */
	bool IsCaptureInProgress() const;

	/** Set focus point (for interactive adjustment) */
	void SetFocusPoint(const FVector& NewFocusPoint);

	/** Calculate optimal configuration for selected actors */
	void CalculateOptimalConfigForSelection();

	/** Preview single viewpoint */
	void PreviewViewpoint(int32 ViewpointIndex);

	/** Get estimated capture stats */
	void GetCaptureStats(int32& OutViewpoints, float& OutEstimatedTime, int64& OutEstimatedDiskSpace) const;

protected:
	/** Draw camera frustum at viewpoint */
	void DrawCameraFrustum(FPrimitiveDrawInterface* PDI, const FCameraViewpoint& Viewpoint, const FLinearColor& Color) const;

	/** Draw trajectory path lines */
	void DrawTrajectoryPath(FPrimitiveDrawInterface* PDI) const;

	/** Draw focus point indicator */
	void DrawFocusPoint(FPrimitiveDrawInterface* PDI) const;

	/** Draw orbital rings (for Orbital trajectory type) */
	void DrawOrbitalRings(FPrimitiveDrawInterface* PDI) const;

	/** Handle capture progress callback */
	void OnCaptureProgressUpdate(int32 CurrentFrame, int32 TotalFrames, float Percent);

	/** Handle capture completion */
	void OnCaptureCompleted(bool bSuccess);

private:
	FCaptureConfig CaptureConfig;
	TArray<FCameraViewpoint> PreviewViewpoints;

	UPROPERTY()
	UCaptureOrchestrator* CaptureOrchestrator;

	int32 SelectedViewpointIndex;
	bool bShowAllFrustums;
	bool bShowTrajectoryPath;
	bool bShowOrbitalRings;
	float FrustumScale;

	FDelegateHandle ProgressDelegateHandle;
	FDelegateHandle CompleteDelegateHandle;
};

/**
 * Toolkit for Gaussian Capture Editor Mode
 */
class UNREALTOGAUSSIAN_API FGaussianCaptureEditorModeToolkit : public FModeToolkit
{
public:
	FGaussianCaptureEditorModeToolkit();

	// FModeToolkit interface
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual FName GetToolkitFName() const override { return FName("GaussianCaptureEditorModeToolkit"); }
	virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("GaussianCapture", "ToolkitName", "3DGS Capture"); }
	virtual TSharedPtr<SWidget> GetInlineContent() const override { return ToolkitWidget; }

private:
	TSharedPtr<SWidget> ToolkitWidget;

	FGaussianCaptureEditorMode* GetEditorMode() const;

	// UI Builders
	TSharedRef<SWidget> BuildTrajectoryPanel();
	TSharedRef<SWidget> BuildCapturePanel();
	TSharedRef<SWidget> BuildPreviewPanel();
	TSharedRef<SWidget> BuildExportPanel();

	// UI Callbacks
	FReply OnStartCaptureClicked();
	FReply OnStopCaptureClicked();
	FReply OnCalculateOptimalClicked();
	FReply OnPreviewTrajectoryClicked();
	FReply OnBrowseOutputClicked();

	void OnTrajectoryTypeChanged(ECameraTrajectoryType NewType);
	void OnNumRingsChanged(int32 NewValue);
	void OnViewsPerRingChanged(int32 NewValue);
	void OnRadiusChanged(float NewValue);
	void OnFocusPointChanged(FVector NewValue);
	void OnImageResolutionChanged(FIntPoint NewResolution);
	void OnFOVChanged(float NewFOV);
};
