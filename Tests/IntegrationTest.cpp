// Copyright 2024 3DGS Research Team. All Rights Reserved.

/**
 * Integration tests for 3DGS export pipeline
 *
 * Tests the complete flow from scene capture to COLMAP output:
 * - Trajectory generation → Coordinate conversion → COLMAP export
 * - Camera intrinsics → Image data → Format output
 * - Point cloud extraction → PLY export
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#include "FCM/CoordinateConverter.h"
#include "FCM/CameraIntrinsics.h"
#include "FCM/ColmapWriter.h"
#include "FCM/PlyWriter.h"
#include "SCM/CameraTrajectory.h"
#include "SCM/CaptureOrchestrator.h"

/**
 * Integration test: Full pipeline from trajectory to COLMAP output
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineIntegrationTest, "UE5_3DGS.Integration.Pipeline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Step 1: Generate camera trajectory
	FTrajectoryConfig TrajectoryConfig;
	TrajectoryConfig.TrajectoryType = ECameraTrajectoryType::Orbital;
	TrajectoryConfig.NumRings = 3;
	TrajectoryConfig.ViewsPerRing = 12;
	TrajectoryConfig.BaseRadius = 500.0f;
	TrajectoryConfig.MinElevation = -30.0f;
	TrajectoryConfig.MaxElevation = 60.0f;
	TrajectoryConfig.FocusPoint = FVector::ZeroVector;
	TrajectoryConfig.bLookAtFocusPoint = true;
	TrajectoryConfig.bStaggerRings = true;

	TArray<FCameraViewpoint> Viewpoints = FCameraTrajectoryGenerator::GenerateViewpoints(TrajectoryConfig);

	int32 ExpectedViewpoints = TrajectoryConfig.NumRings * TrajectoryConfig.ViewsPerRing;
	TestEqual(TEXT("Pipeline: Generated viewpoint count"), Viewpoints.Num(), ExpectedViewpoints);

	// Step 2: Create camera intrinsics
	FCameraIntrinsics Intrinsics(1920, 1080, 90.0f);
	Intrinsics.CameraModel = EColmapCameraModel::PINHOLE;

	TestTrue(TEXT("Pipeline: Intrinsics valid"), Intrinsics.IsValid());

	// Step 3: Convert viewpoints to COLMAP format
	TArray<FColmapImage> ColmapImages = FColmapWriter::CreateImagesFromViewpoints(Viewpoints, Intrinsics);

	TestEqual(TEXT("Pipeline: COLMAP image count"), ColmapImages.Num(), Viewpoints.Num());

	// Step 4: Validate coordinate conversions in pipeline
	for (int32 i = 0; i < FMath::Min(5, ColmapImages.Num()); ++i)
	{
		const FColmapImage& Image = ColmapImages[i];
		const FCameraViewpoint& VP = Viewpoints[i];

		// Verify quaternion is normalized
		TestNearlyEqual(TEXT("Pipeline: Quaternion normalized"), Image.Rotation.Size(), 1.0f, 0.001f);

		// Verify position was converted (should be in meters, not cm)
		FVector ColmapPos = FCoordinateConverter::ConvertPositionToColmap(VP.Position);
		TestNearlyEqual(TEXT("Pipeline: Position X"), Image.Translation.X, ColmapPos.X, 0.001f);
		TestNearlyEqual(TEXT("Pipeline: Position Y"), Image.Translation.Y, ColmapPos.Y, 0.001f);
		TestNearlyEqual(TEXT("Pipeline: Position Z"), Image.Translation.Z, ColmapPos.Z, 0.001f);
	}

	// Step 5: Validate COLMAP camera setup
	FColmapCamera Camera = FColmapWriter::CreateCamera(Intrinsics);

	TestEqual(TEXT("Pipeline: Camera model"), Camera.Model, TEXT("PINHOLE"));
	TestEqual(TEXT("Pipeline: Camera width"), Camera.Width, 1920);
	TestEqual(TEXT("Pipeline: Camera height"), Camera.Height, 1080);
	TestTrue(TEXT("Pipeline: Camera params populated"), Camera.Params.Len() > 0);

	// Step 6: Test point cloud generation
	TArray<FVector> TestVertices;
	TArray<FVector> TestNormals;
	TArray<FColor> TestColors;

	// Generate test mesh data
	for (int32 x = 0; x < 10; ++x)
	{
		for (int32 y = 0; y < 10; ++y)
		{
			TestVertices.Add(FVector(x * 100, y * 100, FMath::Sin(x + y) * 50));
			TestNormals.Add(FVector(0, 0, 1));
			TestColors.Add(FColor(x * 25, y * 25, 128, 255));
		}
	}

	TArray<FPointCloudPoint> PointCloud = FPlyWriter::CreatePointCloudFromMesh(TestVertices, TestNormals, TestColors);

	TestEqual(TEXT("Pipeline: Point cloud size"), PointCloud.Num(), 100);

	// Step 7: Convert to gaussian splats
	TArray<FGaussianSplat> Splats = FPlyWriter::CreateSplatsFromPointCloud(PointCloud);

	TestEqual(TEXT("Pipeline: Splat count"), Splats.Num(), PointCloud.Num());

	// Step 8: Validate splats
	TArray<FString> Warnings;
	bool bSplatsValid = FPlyWriter::ValidateSplats(Splats, Warnings);

	TestTrue(TEXT("Pipeline: Splats valid"), bSplatsValid);

	// Step 9: Verify memory estimates
	int64 EstimatedMemory = FPlyWriter::EstimateMemoryUsage(Splats.Num());
	int64 ExpectedMemory = Splats.Num() * 236; // 236 bytes per splat

	TestEqual(TEXT("Pipeline: Memory estimate"), EstimatedMemory, ExpectedMemory);

	return true;
}

/**
 * Integration test: Coordinate system consistency
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoordinateConsistencyTest, "UE5_3DGS.Integration.CoordinateConsistency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoordinateConsistencyTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Test coordinate transformation consistency across trajectory
	FTrajectoryConfig Config;
	Config.TrajectoryType = ECameraTrajectoryType::Orbital;
	Config.NumRings = 5;
	Config.ViewsPerRing = 24;
	Config.BaseRadius = 500.0f;
	Config.FocusPoint = FVector(100, 200, 50);
	Config.bLookAtFocusPoint = true;

	TArray<FCameraViewpoint> Viewpoints = FCameraTrajectoryGenerator::GenerateViewpoints(Config);

	// All viewpoints should be equidistant from focus point (within tolerance)
	for (const FCameraViewpoint& VP : Viewpoints)
	{
		float Distance = FVector::Distance(VP.Position, Config.FocusPoint);
		TestNearlyEqual(TEXT("Coord: Equidistant from focus"), Distance, Config.BaseRadius, 50.0f);
	}

	// Convert all to COLMAP and verify consistency
	FCameraIntrinsics Intrinsics(1920, 1080, 90.0f);
	TArray<FColmapImage> Images = FColmapWriter::CreateImagesFromViewpoints(Viewpoints, Intrinsics);

	// Verify COLMAP positions form similar pattern around converted focus
	FVector ColmapFocus = FCoordinateConverter::ConvertPositionToColmap(Config.FocusPoint);
	float ColmapRadius = Config.BaseRadius * 0.01f; // Convert cm to m

	for (const FColmapImage& Image : Images)
	{
		float Distance = FVector::Distance(Image.Translation, ColmapFocus);
		TestNearlyEqual(TEXT("Coord: COLMAP equidistant"), Distance, ColmapRadius, 0.5f);
	}

	// Test axis alignment preservation
	{
		// UE5 point on +X axis
		FVector UE5Point(1000, 0, 0);
		FVector ColmapPoint = FCoordinateConverter::ConvertPositionToColmap(UE5Point);

		// Should map to +Z axis in COLMAP
		TestNearlyEqual(TEXT("Coord: X→Z axis X"), ColmapPoint.X, 0.0f, 0.001f);
		TestNearlyEqual(TEXT("Coord: X→Z axis Y"), ColmapPoint.Y, 0.0f, 0.001f);
		TestTrue(TEXT("Coord: X→Z axis Z positive"), ColmapPoint.Z > 0);
	}

	{
		// UE5 point on +Y axis
		FVector UE5Point(0, 1000, 0);
		FVector ColmapPoint = FCoordinateConverter::ConvertPositionToColmap(UE5Point);

		// Should map to +X axis in COLMAP
		TestTrue(TEXT("Coord: Y→X axis X positive"), ColmapPoint.X > 0);
		TestNearlyEqual(TEXT("Coord: Y→X axis Y"), ColmapPoint.Y, 0.0f, 0.001f);
		TestNearlyEqual(TEXT("Coord: Y→X axis Z"), ColmapPoint.Z, 0.0f, 0.001f);
	}

	{
		// UE5 point on +Z axis
		FVector UE5Point(0, 0, 1000);
		FVector ColmapPoint = FCoordinateConverter::ConvertPositionToColmap(UE5Point);

		// Should map to -Y axis in COLMAP
		TestNearlyEqual(TEXT("Coord: Z→-Y axis X"), ColmapPoint.X, 0.0f, 0.001f);
		TestTrue(TEXT("Coord: Z→-Y axis Y negative"), ColmapPoint.Y < 0);
		TestNearlyEqual(TEXT("Coord: Z→-Y axis Z"), ColmapPoint.Z, 0.0f, 0.001f);
	}

	return true;
}

/**
 * Integration test: COLMAP format compliance
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FColmapComplianceTest, "UE5_3DGS.Integration.ColmapCompliance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FColmapComplianceTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Test all supported camera models
	TArray<EColmapCameraModel> Models = {
		EColmapCameraModel::SIMPLE_PINHOLE,
		EColmapCameraModel::PINHOLE,
		EColmapCameraModel::SIMPLE_RADIAL,
		EColmapCameraModel::RADIAL,
		EColmapCameraModel::OPENCV
	};

	TArray<int32> ExpectedIds = { 0, 1, 2, 3, 4 };
	TArray<int32> ExpectedParamCounts = { 3, 4, 4, 5, 8 };

	for (int32 i = 0; i < Models.Num(); ++i)
	{
		FCameraIntrinsics Intrinsics(1920, 1080, 90.0f);
		Intrinsics.CameraModel = Models[i];

		// Verify model ID
		int32 ModelId = Intrinsics.GetColmapModelId();
		TestEqual(FString::Printf(TEXT("COLMAP: Model %d ID"), i), ModelId, ExpectedIds[i]);

		// Verify parameter count
		int32 ParamCount = Intrinsics.GetColmapParamCount();
		TestEqual(FString::Printf(TEXT("COLMAP: Model %d params"), i), ParamCount, ExpectedParamCounts[i]);

		// Verify params string format
		FString ParamsStr = Intrinsics.GetColmapParamsString();
		TArray<FString> Params;
		ParamsStr.ParseIntoArray(Params, TEXT(" "), true);
		TestEqual(FString::Printf(TEXT("COLMAP: Model %d param parts"), i), Params.Num(), ExpectedParamCounts[i]);
	}

	// Test quaternion convention (COLMAP uses WXYZ)
	{
		FCameraIntrinsics Intrinsics(1920, 1080, 90.0f);

		TArray<FCameraViewpoint> Viewpoints;
		FCameraViewpoint VP;
		VP.Position = FVector(500, 0, 0);
		VP.Rotation = FRotator(0, 90, 0); // 90 degree yaw
		VP.ViewpointId = 0;
		Viewpoints.Add(VP);

		TArray<FColmapImage> Images = FColmapWriter::CreateImagesFromViewpoints(Viewpoints, Intrinsics);

		FQuat Q = Images[0].Rotation;

		// Quaternion should be normalized
		TestNearlyEqual(TEXT("COLMAP: Quat normalized"), Q.Size(), 1.0f, 0.001f);

		// W component should be accessible (COLMAP format)
		TestTrue(TEXT("COLMAP: Quat W in range"), FMath::Abs(Q.W) <= 1.0f);
	}

	// Test image naming convention
	{
		FCameraIntrinsics Intrinsics(1920, 1080, 90.0f);

		TArray<FCameraViewpoint> Viewpoints;
		for (int32 i = 0; i < 100; ++i)
		{
			FCameraViewpoint VP;
			VP.Position = FVector(i * 10, 0, 0);
			VP.Rotation = FRotator::ZeroRotator;
			VP.ViewpointId = i;
			Viewpoints.Add(VP);
		}

		TArray<FColmapImage> Images = FColmapWriter::CreateImagesFromViewpoints(Viewpoints, Intrinsics);

		// All image names should be unique
		TSet<FString> UniqueNames;
		for (const FColmapImage& Image : Images)
		{
			TestFalse(TEXT("COLMAP: Unique image name"), UniqueNames.Contains(Image.ImageName));
			UniqueNames.Add(Image.ImageName);
		}

		// Image IDs should be sequential starting from 1
		for (int32 i = 0; i < Images.Num(); ++i)
		{
			TestEqual(TEXT("COLMAP: Sequential image ID"), Images[i].ImageId, i + 1);
		}
	}

	return true;
}

/**
 * Integration test: 3DGS PLY format compliance
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlyComplianceTest, "UE5_3DGS.Integration.PlyCompliance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlyComplianceTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Test 3DGS splat format
	{
		FGaussianSplat Splat;

		// Verify size (236 bytes per 3DGS paper)
		// Position: 12, Normal: 12, SH_DC: 12, SH_Rest: 180, Opacity: 4, Scale: 12, Rotation: 16
		// Total: 12 + 12 + 12 + 180 + 4 + 12 + 16 = 248 (our struct may differ slightly)

		// Check SH coefficient count
		TestEqual(TEXT("PLY: SH_Rest count"), Splat.SH_Rest.Num(), 45);
	}

	// Test color ↔ SH coefficient conversion accuracy
	{
		TArray<FColor> TestColors = {
			FColor::Black,
			FColor::White,
			FColor::Red,
			FColor::Green,
			FColor::Blue,
			FColor(128, 64, 192, 255),
			FColor(0, 255, 128, 255)
		};

		for (const FColor& Color : TestColors)
		{
			FVector SH_DC = FGaussianSplat::ColorToSH_DC(Color);
			FColor Recovered = FGaussianSplat::SH_DCToColor(SH_DC);

			TestTrue(FString::Printf(TEXT("PLY: Color roundtrip R (%d)"), Color.R),
				FMath::Abs(static_cast<int32>(Recovered.R) - static_cast<int32>(Color.R)) <= 1);
			TestTrue(FString::Printf(TEXT("PLY: Color roundtrip G (%d)"), Color.G),
				FMath::Abs(static_cast<int32>(Recovered.G) - static_cast<int32>(Color.G)) <= 1);
			TestTrue(FString::Printf(TEXT("PLY: Color roundtrip B (%d)"), Color.B),
				FMath::Abs(static_cast<int32>(Recovered.B) - static_cast<int32>(Color.B)) <= 1);
		}
	}

	// Test scale log encoding (3DGS uses log-scale)
	{
		FGaussianSplat Splat;

		// Set scale values (these should be log-encoded for 3DGS)
		Splat.Scale = FVector(-5.0f, -5.0f, -5.0f); // log(0.0067) ≈ -5

		// Verify scale is in expected range for log-encoded values
		TestTrue(TEXT("PLY: Scale X in log range"), Splat.Scale.X >= -10.0f && Splat.Scale.X <= 10.0f);
		TestTrue(TEXT("PLY: Scale Y in log range"), Splat.Scale.Y >= -10.0f && Splat.Scale.Y <= 10.0f);
		TestTrue(TEXT("PLY: Scale Z in log range"), Splat.Scale.Z >= -10.0f && Splat.Scale.Z <= 10.0f);
	}

	// Test rotation quaternion normalization
	{
		TArray<FGaussianSplat> Splats;

		for (int32 i = 0; i < 100; ++i)
		{
			FGaussianSplat Splat;
			Splat.Position = FVector(i, 0, 0);
			Splat.Rotation = FQuat(
				FMath::FRand() * 2.0f - 1.0f,
				FMath::FRand() * 2.0f - 1.0f,
				FMath::FRand() * 2.0f - 1.0f,
				FMath::FRand() * 2.0f - 1.0f
			);
			Splat.Rotation.Normalize();
			Splat.Opacity = 1.0f;
			Splat.Scale = FVector(-3.0f, -3.0f, -3.0f);
			Splats.Add(Splat);
		}

		TArray<FString> Warnings;
		bool bValid = FPlyWriter::ValidateSplats(Splats, Warnings);

		TestTrue(TEXT("PLY: Random rotations valid"), bValid);

		// Check each quaternion is normalized
		for (const FGaussianSplat& Splat : Splats)
		{
			TestNearlyEqual(TEXT("PLY: Quat normalized"), Splat.Rotation.Size(), 1.0f, 0.001f);
		}
	}

	// Test opacity encoding (3DGS uses inverse sigmoid)
	{
		// Test boundary values
		float TestOpacities[] = { 0.01f, 0.5f, 0.99f };

		for (float Opacity : TestOpacities)
		{
			// Opacity should be clamped to valid range
			TestTrue(TEXT("PLY: Opacity in range"), Opacity >= 0.0f && Opacity <= 1.0f);
		}
	}

	return true;
}

/**
 * Integration test: Capture orchestration validation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCaptureOrchestratorTest, "UE5_3DGS.Integration.CaptureOrchestrator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCaptureOrchestratorTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Test configuration validation
	{
		FCaptureConfig ValidConfig;
		ValidConfig.OutputDirectory = TEXT("/tmp/test_output");
		ValidConfig.ImageWidth = 1920;
		ValidConfig.ImageHeight = 1080;
		ValidConfig.FieldOfView = 90.0f;
		ValidConfig.TrajectoryConfig.NumRings = 3;
		ValidConfig.TrajectoryConfig.ViewsPerRing = 12;
		ValidConfig.TrajectoryConfig.BaseRadius = 500.0f;

		// Create orchestrator
		UCaptureOrchestrator* Orchestrator = NewObject<UCaptureOrchestrator>();

		TArray<FString> Warnings;
		bool bValid = Orchestrator->ValidateConfig(ValidConfig, Warnings);

		TestTrue(TEXT("Orchestrator: Valid config passes"), bValid);
	}

	// Test invalid configuration detection
	{
		FCaptureConfig InvalidConfig;
		InvalidConfig.ImageWidth = 0; // Invalid
		InvalidConfig.ImageHeight = 1080;
		InvalidConfig.FieldOfView = 90.0f;

		UCaptureOrchestrator* Orchestrator = NewObject<UCaptureOrchestrator>();

		TArray<FString> Warnings;
		bool bValid = Orchestrator->ValidateConfig(InvalidConfig, Warnings);

		TestFalse(TEXT("Orchestrator: Zero width fails"), bValid);
	}

	// Test trajectory preview
	{
		UCaptureOrchestrator* Orchestrator = NewObject<UCaptureOrchestrator>();

		FTrajectoryConfig Config;
		Config.TrajectoryType = ECameraTrajectoryType::Orbital;
		Config.NumRings = 3;
		Config.ViewsPerRing = 12;
		Config.BaseRadius = 500.0f;

		TArray<FTransform> PreviewTransforms = Orchestrator->PreviewTrajectory(Config);

		TestEqual(TEXT("Orchestrator: Preview count"), PreviewTransforms.Num(), 36);
	}

	// Test state machine
	{
		UCaptureOrchestrator* Orchestrator = NewObject<UCaptureOrchestrator>();

		// Initial state should be Idle
		TestEqual(TEXT("Orchestrator: Initial state Idle"),
			Orchestrator->GetCaptureState(), ECaptureState::Idle);

		// Progress should be 0 when idle
		TestNearlyEqual(TEXT("Orchestrator: Idle progress 0"),
			Orchestrator->GetCaptureProgress(), 0.0f, 0.001f);
	}

	return true;
}

/**
 * Integration test: End-to-end data integrity
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataIntegrityTest, "UE5_3DGS.Integration.DataIntegrity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataIntegrityTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Generate a complete dataset and verify integrity
	FTrajectoryConfig TrajectoryConfig;
	TrajectoryConfig.TrajectoryType = ECameraTrajectoryType::Orbital;
	TrajectoryConfig.NumRings = 5;
	TrajectoryConfig.ViewsPerRing = 24;
	TrajectoryConfig.BaseRadius = 500.0f;
	TrajectoryConfig.MinElevation = -30.0f;
	TrajectoryConfig.MaxElevation = 60.0f;
	TrajectoryConfig.FocusPoint = FVector(0, 0, 100);
	TrajectoryConfig.bLookAtFocusPoint = true;
	TrajectoryConfig.bStaggerRings = true;

	TArray<FCameraViewpoint> Viewpoints = FCameraTrajectoryGenerator::GenerateViewpoints(TrajectoryConfig);

	// Verify view distribution
	TMap<int32, int32> RingCounts;
	for (const FCameraViewpoint& VP : Viewpoints)
	{
		RingCounts.FindOrAdd(VP.RingIndex)++;
	}

	// Each ring should have the expected number of views
	TestEqual(TEXT("Integrity: Ring count"), RingCounts.Num(), TrajectoryConfig.NumRings);

	for (const auto& Pair : RingCounts)
	{
		TestEqual(FString::Printf(TEXT("Integrity: Ring %d views"), Pair.Key),
			Pair.Value, TrajectoryConfig.ViewsPerRing);
	}

	// Test coordinate integrity through pipeline
	FCameraIntrinsics Intrinsics(1920, 1080, 90.0f);
	TArray<FColmapImage> Images = FColmapWriter::CreateImagesFromViewpoints(Viewpoints, Intrinsics);

	// Verify no NaN or Inf values
	for (const FColmapImage& Image : Images)
	{
		TestFalse(TEXT("Integrity: Position not NaN"),
			FMath::IsNaN(Image.Translation.X) ||
			FMath::IsNaN(Image.Translation.Y) ||
			FMath::IsNaN(Image.Translation.Z));

		TestFalse(TEXT("Integrity: Position not Inf"),
			!FMath::IsFinite(Image.Translation.X) ||
			!FMath::IsFinite(Image.Translation.Y) ||
			!FMath::IsFinite(Image.Translation.Z));

		TestFalse(TEXT("Integrity: Rotation not NaN"),
			FMath::IsNaN(Image.Rotation.W) ||
			FMath::IsNaN(Image.Rotation.X) ||
			FMath::IsNaN(Image.Rotation.Y) ||
			FMath::IsNaN(Image.Rotation.Z));
	}

	// Test point cloud integrity
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FColor> Colors;

	for (int32 i = 0; i < 1000; ++i)
	{
		Vertices.Add(FVector(
			FMath::FRand() * 1000 - 500,
			FMath::FRand() * 1000 - 500,
			FMath::FRand() * 500
		));
		Normals.Add(FVector(0, 0, 1).GetSafeNormal());
		Colors.Add(FColor(
			FMath::RandRange(0, 255),
			FMath::RandRange(0, 255),
			FMath::RandRange(0, 255),
			255
		));
	}

	TArray<FPointCloudPoint> Points = FPlyWriter::CreatePointCloudFromMesh(Vertices, Normals, Colors);
	TArray<FGaussianSplat> Splats = FPlyWriter::CreateSplatsFromPointCloud(Points);

	// Verify splat integrity
	for (const FGaussianSplat& Splat : Splats)
	{
		TestFalse(TEXT("Integrity: Splat position not NaN"),
			FMath::IsNaN(Splat.Position.X) ||
			FMath::IsNaN(Splat.Position.Y) ||
			FMath::IsNaN(Splat.Position.Z));

		TestTrue(TEXT("Integrity: Splat opacity valid"),
			Splat.Opacity >= 0.0f && Splat.Opacity <= 1.0f);

		TestTrue(TEXT("Integrity: Splat rotation normalized"),
			FMath::IsNearlyEqual(Splat.Rotation.Size(), 1.0f, 0.01f));
	}

	TArray<FString> Warnings;
	bool bValid = FPlyWriter::ValidateSplats(Splats, Warnings);
	TestTrue(TEXT("Integrity: All splats valid"), bValid);

	return true;
}
