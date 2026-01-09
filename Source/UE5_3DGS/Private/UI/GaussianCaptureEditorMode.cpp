// Copyright 2024 3DGS Research Team. All Rights Reserved.

#include "UI/GaussianCaptureEditorMode.h"
#include "SCM/CameraTrajectory.h"
#include "EditorModeManager.h"
#include "Toolkits/ToolkitManager.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "DesktopPlatformModule.h"
#include "Misc/Paths.h"

const FEditorModeID FGaussianCaptureEditorMode::EM_GaussianCapture(TEXT("EM_GaussianCapture"));

FGaussianCaptureEditorMode::FGaussianCaptureEditorMode()
	: CaptureOrchestrator(nullptr)
	, SelectedViewpointIndex(-1)
	, bShowAllFrustums(true)
	, bShowTrajectoryPath(true)
	, bShowOrbitalRings(true)
	, FrustumScale(50.0f)
{
	// Default configuration
	CaptureConfig.OutputDirectory = FPaths::ProjectSavedDir() / TEXT("3DGS_Export");
	CaptureConfig.ImageWidth = 1920;
	CaptureConfig.ImageHeight = 1080;
	CaptureConfig.FieldOfView = 90.0f;

	// Default trajectory
	CaptureConfig.TrajectoryConfig.TrajectoryType = ECameraTrajectoryType::Orbital;
	CaptureConfig.TrajectoryConfig.NumRings = 5;
	CaptureConfig.TrajectoryConfig.ViewsPerRing = 24;
	CaptureConfig.TrajectoryConfig.BaseRadius = 500.0f;
	CaptureConfig.TrajectoryConfig.MinElevation = -30.0f;
	CaptureConfig.TrajectoryConfig.MaxElevation = 60.0f;
	CaptureConfig.TrajectoryConfig.bStaggerRings = true;
	CaptureConfig.TrajectoryConfig.bLookAtFocusPoint = true;
}

FGaussianCaptureEditorMode::~FGaussianCaptureEditorMode()
{
}

void FGaussianCaptureEditorMode::Enter()
{
	FEdMode::Enter();

	// Create capture orchestrator
	CaptureOrchestrator = NewObject<UCaptureOrchestrator>();

	// Generate initial trajectory preview
	RegenerateTrajectory();

	// Create toolkit
	if (!Toolkit.IsValid())
	{
		Toolkit = MakeShareable(new FGaussianCaptureEditorModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost(), GetModeManager()->GetActiveMode(GetID()));
	}

	UE_LOG(LogTemp, Log, TEXT("Entered 3DGS Capture Editor Mode"));
}

void FGaussianCaptureEditorMode::Exit()
{
	if (CaptureOrchestrator && CaptureOrchestrator->GetCaptureState() != ECaptureState::Idle)
	{
		CaptureOrchestrator->StopCapture();
	}

	CaptureOrchestrator = nullptr;

	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	FEdMode::Exit();
	UE_LOG(LogTemp, Log, TEXT("Exited 3DGS Capture Editor Mode"));
}

void FGaussianCaptureEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);

	// Update capture progress if in progress
	if (CaptureOrchestrator && CaptureOrchestrator->GetCaptureState() == ECaptureState::Capturing)
	{
		// Progress is handled by delegate
	}
}

void FGaussianCaptureEditorMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

	if (PreviewViewpoints.Num() == 0)
	{
		return;
	}

	// Draw focus point
	DrawFocusPoint(PDI);

	// Draw orbital rings if applicable
	if (bShowOrbitalRings && CaptureConfig.TrajectoryConfig.TrajectoryType == ECameraTrajectoryType::Orbital)
	{
		DrawOrbitalRings(PDI);
	}

	// Draw trajectory path
	if (bShowTrajectoryPath)
	{
		DrawTrajectoryPath(PDI);
	}

	// Draw camera frustums
	for (int32 i = 0; i < PreviewViewpoints.Num(); ++i)
	{
		const FCameraViewpoint& VP = PreviewViewpoints[i];

		FLinearColor Color = FLinearColor::White;
		if (i == SelectedViewpointIndex)
		{
			Color = FLinearColor::Yellow;
		}
		else if (!bShowAllFrustums && i != SelectedViewpointIndex)
		{
			continue;
		}
		else
		{
			// Color by ring
			float Hue = (VP.RingIndex * 60.0f) / 360.0f;
			Color = FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue * 255), 200, 255);
		}

		DrawCameraFrustum(PDI, VP, Color);
	}
}

bool FGaussianCaptureEditorMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (Click.GetKey() == EKeys::LeftMouseButton && Click.IsControlDown())
	{
		// Set focus point on Ctrl+Click
		FVector WorldLocation, WorldDirection;
		InViewportClient->DeprojectMousePositionToWorldRay(Click.GetClickPos(), WorldLocation, WorldDirection);

		FHitResult HitResult;
		if (InViewportClient->GetWorld()->LineTraceSingleByChannel(
			HitResult,
			WorldLocation,
			WorldLocation + WorldDirection * 100000.0f,
			ECC_Visibility))
		{
			SetFocusPoint(HitResult.Location);
			return true;
		}
	}

	return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
}

bool FGaussianCaptureEditorMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Event == IE_Pressed)
	{
		if (Key == EKeys::R)
		{
			// Regenerate trajectory
			RegenerateTrajectory();
			return true;
		}
		else if (Key == EKeys::F)
		{
			// Toggle frustum display
			bShowAllFrustums = !bShowAllFrustums;
			return true;
		}
		else if (Key == EKeys::P)
		{
			// Toggle path display
			bShowTrajectoryPath = !bShowTrajectoryPath;
			return true;
		}
		else if (Key == EKeys::PageUp && SelectedViewpointIndex > 0)
		{
			SelectedViewpointIndex--;
			return true;
		}
		else if (Key == EKeys::PageDown && SelectedViewpointIndex < PreviewViewpoints.Num() - 1)
		{
			SelectedViewpointIndex++;
			return true;
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

bool FGaussianCaptureEditorMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	return OtherModeID == FBuiltinEditorModes::EM_Default;
}

void FGaussianCaptureEditorMode::SetCaptureConfig(const FCaptureConfig& Config)
{
	CaptureConfig = Config;
	RegenerateTrajectory();
}

void FGaussianCaptureEditorMode::SetTrajectoryConfig(const FTrajectoryConfig& Config)
{
	CaptureConfig.TrajectoryConfig = Config;
	RegenerateTrajectory();
}

void FGaussianCaptureEditorMode::RegenerateTrajectory()
{
	PreviewViewpoints = UE5_3DGS::FCameraTrajectoryGenerator::GenerateViewpoints(CaptureConfig.TrajectoryConfig);

	if (SelectedViewpointIndex >= PreviewViewpoints.Num())
	{
		SelectedViewpointIndex = PreviewViewpoints.Num() - 1;
	}

	UE_LOG(LogTemp, Log, TEXT("Generated %d preview viewpoints"), PreviewViewpoints.Num());
}

bool FGaussianCaptureEditorMode::StartCapture()
{
	if (!CaptureOrchestrator)
	{
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return false;
	}

	// Bind delegates
	CaptureOrchestrator->OnCaptureProgress.AddDynamic(this, &FGaussianCaptureEditorMode::OnCaptureProgressUpdate);
	CaptureOrchestrator->OnCaptureComplete.AddDynamic(this, &FGaussianCaptureEditorMode::OnCaptureCompleted);

	return CaptureOrchestrator->StartCapture(World, CaptureConfig);
}

void FGaussianCaptureEditorMode::StopCapture()
{
	if (CaptureOrchestrator)
	{
		CaptureOrchestrator->StopCapture();
	}
}

float FGaussianCaptureEditorMode::GetCaptureProgress() const
{
	return CaptureOrchestrator ? CaptureOrchestrator->GetCaptureProgress() : 0.0f;
}

bool FGaussianCaptureEditorMode::IsCaptureInProgress() const
{
	return CaptureOrchestrator && CaptureOrchestrator->GetCaptureState() == ECaptureState::Capturing;
}

void FGaussianCaptureEditorMode::SetFocusPoint(const FVector& NewFocusPoint)
{
	CaptureConfig.TrajectoryConfig.FocusPoint = NewFocusPoint;
	RegenerateTrajectory();
}

void FGaussianCaptureEditorMode::CalculateOptimalConfigForSelection()
{
	USelection* Selection = GEditor->GetSelectedActors();
	if (Selection->Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No actors selected for optimal config calculation"));
		return;
	}

	// Calculate bounding box of selection
	FBox SelectionBounds(ForceInit);
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			FBox ActorBounds = Actor->GetComponentsBoundingBox(true);
			SelectionBounds += ActorBounds;
		}
	}

	if (SelectionBounds.IsValid)
	{
		FTrajectoryConfig OptimalConfig = UE5_3DGS::FCameraTrajectoryGenerator::CalculateOptimalConfig(
			SelectionBounds,
			0.7f, // 70% overlap
			CaptureConfig.FieldOfView
		);

		CaptureConfig.TrajectoryConfig = OptimalConfig;
		RegenerateTrajectory();

		UE_LOG(LogTemp, Log, TEXT("Calculated optimal config: Radius=%.1f, Rings=%d, Views=%d"),
			OptimalConfig.BaseRadius, OptimalConfig.NumRings, OptimalConfig.ViewsPerRing);
	}
}

void FGaussianCaptureEditorMode::PreviewViewpoint(int32 ViewpointIndex)
{
	if (ViewpointIndex >= 0 && ViewpointIndex < PreviewViewpoints.Num())
	{
		SelectedViewpointIndex = ViewpointIndex;

		// Move editor viewport to this viewpoint
		if (GEditor && GEditor->GetActiveViewport())
		{
			const FCameraViewpoint& VP = PreviewViewpoints[ViewpointIndex];
			FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
			if (ViewportClient)
			{
				ViewportClient->SetViewLocation(VP.Position);
				ViewportClient->SetViewRotation(VP.Rotation);
			}
		}
	}
}

void FGaussianCaptureEditorMode::GetCaptureStats(int32& OutViewpoints, float& OutEstimatedTime, int64& OutEstimatedDiskSpace) const
{
	OutViewpoints = PreviewViewpoints.Num();

	// Estimate 0.5 seconds per frame (capture + save)
	OutEstimatedTime = OutViewpoints * 0.5f;

	// Estimate disk space: image + depth per frame
	int64 ImageSize = CaptureConfig.ImageWidth * CaptureConfig.ImageHeight * 3; // RGB
	if (CaptureConfig.ImageFormat == EImageFormat::JPEG)
	{
		ImageSize = ImageSize / 10; // ~10:1 JPEG compression
	}

	int64 DepthSize = CaptureConfig.bCaptureDepth ?
		CaptureConfig.ImageWidth * CaptureConfig.ImageHeight * 4 : 0; // Float32

	OutEstimatedDiskSpace = (ImageSize + DepthSize) * OutViewpoints;
}

void FGaussianCaptureEditorMode::DrawCameraFrustum(FPrimitiveDrawInterface* PDI, const FCameraViewpoint& Viewpoint, const FLinearColor& Color) const
{
	float FOVRad = FMath::DegreesToRadians(CaptureConfig.FieldOfView);
	float AspectRatio = static_cast<float>(CaptureConfig.ImageWidth) / CaptureConfig.ImageHeight;

	float HalfAngleH = FOVRad / 2.0f;
	float HalfAngleV = FMath::Atan(FMath::Tan(HalfAngleH) / AspectRatio);

	// Frustum corners at FrustumScale distance
	float NearDist = FrustumScale;
	float HalfWidthNear = NearDist * FMath::Tan(HalfAngleH);
	float HalfHeightNear = NearDist * FMath::Tan(HalfAngleV);

	// Local space corners
	TArray<FVector> LocalCorners = {
		FVector(NearDist, -HalfWidthNear, HalfHeightNear),   // Top left
		FVector(NearDist, HalfWidthNear, HalfHeightNear),    // Top right
		FVector(NearDist, HalfWidthNear, -HalfHeightNear),   // Bottom right
		FVector(NearDist, -HalfWidthNear, -HalfHeightNear)   // Bottom left
	};

	// Transform to world space
	FTransform CameraTransform = Viewpoint.GetTransform();
	FVector CameraPos = CameraTransform.GetLocation();

	TArray<FVector> WorldCorners;
	for (const FVector& LocalCorner : LocalCorners)
	{
		WorldCorners.Add(CameraTransform.TransformPosition(LocalCorner));
	}

	// Draw frustum edges
	for (int32 i = 0; i < 4; ++i)
	{
		PDI->DrawLine(CameraPos, WorldCorners[i], Color, SDPG_Foreground, 1.0f);
		PDI->DrawLine(WorldCorners[i], WorldCorners[(i + 1) % 4], Color, SDPG_Foreground, 1.0f);
	}

	// Draw camera position marker
	PDI->DrawPoint(CameraPos, Color, 5.0f, SDPG_Foreground);
}

void FGaussianCaptureEditorMode::DrawTrajectoryPath(FPrimitiveDrawInterface* PDI) const
{
	if (PreviewViewpoints.Num() < 2)
	{
		return;
	}

	// Draw path within each ring
	int32 CurrentRing = -1;
	for (int32 i = 0; i < PreviewViewpoints.Num(); ++i)
	{
		const FCameraViewpoint& VP = PreviewViewpoints[i];

		if (VP.RingIndex != CurrentRing)
		{
			CurrentRing = VP.RingIndex;
			continue; // Don't connect across rings
		}

		const FCameraViewpoint& PrevVP = PreviewViewpoints[i - 1];
		if (PrevVP.RingIndex == VP.RingIndex)
		{
			FLinearColor PathColor = FLinearColor::Green.Desaturate(0.3f);
			PDI->DrawLine(PrevVP.Position, VP.Position, PathColor, SDPG_World, 0.5f);
		}
	}
}

void FGaussianCaptureEditorMode::DrawFocusPoint(FPrimitiveDrawInterface* PDI) const
{
	FVector FocusPoint = CaptureConfig.TrajectoryConfig.FocusPoint;

	// Draw cross at focus point
	float CrossSize = 20.0f;
	FLinearColor FocusColor = FLinearColor::Red;

	PDI->DrawLine(FocusPoint - FVector(CrossSize, 0, 0), FocusPoint + FVector(CrossSize, 0, 0), FocusColor, SDPG_Foreground, 2.0f);
	PDI->DrawLine(FocusPoint - FVector(0, CrossSize, 0), FocusPoint + FVector(0, CrossSize, 0), FocusColor, SDPG_Foreground, 2.0f);
	PDI->DrawLine(FocusPoint - FVector(0, 0, CrossSize), FocusPoint + FVector(0, 0, CrossSize), FocusColor, SDPG_Foreground, 2.0f);

	// Draw sphere
	DrawWireSphere(PDI, FocusPoint, FocusColor, 10.0f, 16, SDPG_Foreground);
}

void FGaussianCaptureEditorMode::DrawOrbitalRings(FPrimitiveDrawInterface* PDI) const
{
	const FTrajectoryConfig& Config = CaptureConfig.TrajectoryConfig;

	if (Config.NumRings <= 0)
	{
		return;
	}

	float ElevationStep = (Config.MaxElevation - Config.MinElevation) / FMath::Max(Config.NumRings - 1, 1);

	for (int32 RingIdx = 0; RingIdx < Config.NumRings; ++RingIdx)
	{
		float Elevation = Config.MinElevation + (ElevationStep * RingIdx);
		float RingRadius = Config.BaseRadius;

		if (Config.bVaryRadiusPerRing)
		{
			float Variation = FMath::Sin(RingIdx * PI / Config.NumRings);
			RingRadius *= (1.0f + Config.RadiusVariation * Variation);
		}

		// Draw ring circle
		FLinearColor RingColor = FLinearColor::Blue.Desaturate(0.5f);
		RingColor.A = 0.5f;

		int32 NumSegments = 36;
		for (int32 Seg = 0; Seg < NumSegments; ++Seg)
		{
			float Angle1 = (Seg * 360.0f / NumSegments);
			float Angle2 = ((Seg + 1) * 360.0f / NumSegments);

			FVector P1 = UE5_3DGS::FCameraTrajectoryGenerator::SphericalToCartesian(
				RingRadius, Elevation, Angle1, Config.FocusPoint);
			FVector P2 = UE5_3DGS::FCameraTrajectoryGenerator::SphericalToCartesian(
				RingRadius, Elevation, Angle2, Config.FocusPoint);

			PDI->DrawLine(P1, P2, RingColor, SDPG_World, 0.5f);
		}
	}
}

void FGaussianCaptureEditorMode::OnCaptureProgressUpdate(int32 CurrentFrame, int32 TotalFrames, float Percent)
{
	UE_LOG(LogTemp, Log, TEXT("Capture progress: %d/%d (%.1f%%)"), CurrentFrame, TotalFrames, Percent);
}

void FGaussianCaptureEditorMode::OnCaptureCompleted(bool bSuccess)
{
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Capture completed successfully!"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Capture failed or was cancelled"));
	}

	// Cleanup delegates
	if (CaptureOrchestrator)
	{
		CaptureOrchestrator->OnCaptureProgress.RemoveAll(this);
		CaptureOrchestrator->OnCaptureComplete.RemoveAll(this);
	}
}

// Toolkit implementation

FGaussianCaptureEditorModeToolkit::FGaussianCaptureEditorModeToolkit()
{
}

void FGaussianCaptureEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	// Build toolkit widget
	ToolkitWidget = SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("3D Gaussian Splatting Capture")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				BuildTrajectoryPanel()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				BuildCapturePanel()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				BuildPreviewPanel()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				BuildExportPanel()
			]
		];
}

FGaussianCaptureEditorMode* FGaussianCaptureEditorModeToolkit::GetEditorMode() const
{
	return static_cast<FGaussianCaptureEditorMode*>(GetScriptableEditorMode().Get());
}

TSharedRef<SWidget> FGaussianCaptureEditorModeToolkit::BuildTrajectoryPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Trajectory Settings")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Calculate Optimal from Selection")))
			.OnClicked(this, &FGaussianCaptureEditorModeToolkit::OnCalculateOptimalClicked)
		];
}

TSharedRef<SWidget> FGaussianCaptureEditorModeToolkit::BuildCapturePanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Capture Settings")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Browse Output Directory...")))
			.OnClicked(this, &FGaussianCaptureEditorModeToolkit::OnBrowseOutputClicked)
		];
}

TSharedRef<SWidget> FGaussianCaptureEditorModeToolkit::BuildPreviewPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Preview")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Regenerate Trajectory")))
			.OnClicked(this, &FGaussianCaptureEditorModeToolkit::OnPreviewTrajectoryClicked)
		];
}

TSharedRef<SWidget> FGaussianCaptureEditorModeToolkit::BuildExportPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Export")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Start Capture")))
				.OnClicked(this, &FGaussianCaptureEditorModeToolkit::OnStartCaptureClicked)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Stop")))
				.OnClicked(this, &FGaussianCaptureEditorModeToolkit::OnStopCaptureClicked)
			]
		];
}

FReply FGaussianCaptureEditorModeToolkit::OnStartCaptureClicked()
{
	if (FGaussianCaptureEditorMode* Mode = GetEditorMode())
	{
		Mode->StartCapture();
	}
	return FReply::Handled();
}

FReply FGaussianCaptureEditorModeToolkit::OnStopCaptureClicked()
{
	if (FGaussianCaptureEditorMode* Mode = GetEditorMode())
	{
		Mode->StopCapture();
	}
	return FReply::Handled();
}

FReply FGaussianCaptureEditorModeToolkit::OnCalculateOptimalClicked()
{
	if (FGaussianCaptureEditorMode* Mode = GetEditorMode())
	{
		Mode->CalculateOptimalConfigForSelection();
	}
	return FReply::Handled();
}

FReply FGaussianCaptureEditorModeToolkit::OnPreviewTrajectoryClicked()
{
	if (FGaussianCaptureEditorMode* Mode = GetEditorMode())
	{
		Mode->RegenerateTrajectory();
	}
	return FReply::Handled();
}

FReply FGaussianCaptureEditorModeToolkit::OnBrowseOutputClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString OutFolder;
		if (DesktopPlatform->OpenDirectoryDialog(
			nullptr,
			TEXT("Select Output Directory"),
			FPaths::ProjectSavedDir(),
			OutFolder))
		{
			if (FGaussianCaptureEditorMode* Mode = GetEditorMode())
			{
				FCaptureConfig Config = Mode->GetCaptureConfig();
				Config.OutputDirectory = OutFolder;
				Mode->SetCaptureConfig(Config);
			}
		}
	}
	return FReply::Handled();
}
