// Copyright 2024 3DGS Research Team. All Rights Reserved.

/**
 * Unit tests for coordinate conversion utilities
 *
 * Test coverage:
 * - UE5 to COLMAP position conversion
 * - UE5 to COLMAP rotation conversion
 * - Round-trip conversion accuracy
 * - Edge cases (origin, axis-aligned vectors)
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FCM/CoordinateConverter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoordinateConverterPositionTest, "UE5_3DGS.FCM.CoordinateConverter.Position", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoordinateConverterPositionTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Test 1: Origin should map to origin
	{
		FVector UE5Origin(0, 0, 0);
		FVector ColmapOrigin = FCoordinateConverter::ConvertPositionToColmap(UE5Origin);
		TestEqual(TEXT("Origin X"), ColmapOrigin.X, 0.0, 0.0001);
		TestEqual(TEXT("Origin Y"), ColmapOrigin.Y, 0.0, 0.0001);
		TestEqual(TEXT("Origin Z"), ColmapOrigin.Z, 0.0, 0.0001);
	}

	// Test 2: UE5 Forward (+X) should map to COLMAP Forward (+Z)
	{
		FVector UE5Forward(100, 0, 0); // 100cm = 1m
		FVector ColmapPos = FCoordinateConverter::ConvertPositionToColmap(UE5Forward);
		TestEqual(TEXT("Forward maps to +Z"), ColmapPos.Z, 1.0, 0.0001);
		TestNearlyEqual(TEXT("Forward X is 0"), ColmapPos.X, 0.0, 0.0001f);
		TestNearlyEqual(TEXT("Forward Y is 0"), ColmapPos.Y, 0.0, 0.0001f);
	}

	// Test 3: UE5 Right (+Y) should map to COLMAP Right (+X)
	{
		FVector UE5Right(0, 100, 0); // 100cm = 1m
		FVector ColmapPos = FCoordinateConverter::ConvertPositionToColmap(UE5Right);
		TestEqual(TEXT("Right maps to +X"), ColmapPos.X, 1.0, 0.0001);
		TestNearlyEqual(TEXT("Right Z is 0"), ColmapPos.Z, 0.0, 0.0001f);
	}

	// Test 4: UE5 Up (+Z) should map to COLMAP Down (-Y)
	{
		FVector UE5Up(0, 0, 100); // 100cm = 1m
		FVector ColmapPos = FCoordinateConverter::ConvertPositionToColmap(UE5Up);
		TestEqual(TEXT("Up maps to -Y"), ColmapPos.Y, -1.0, 0.0001);
		TestNearlyEqual(TEXT("Up X is 0"), ColmapPos.X, 0.0, 0.0001f);
		TestNearlyEqual(TEXT("Up Z is 0"), ColmapPos.Z, 0.0, 0.0001f);
	}

	// Test 5: Round-trip conversion
	{
		FVector Original(123.456f, -789.012f, 345.678f);
		FVector Colmap = FCoordinateConverter::ConvertPositionToColmap(Original);
		FVector RoundTrip = FCoordinateConverter::ConvertPositionFromColmap(Colmap);

		TestNearlyEqual(TEXT("Round-trip X"), RoundTrip.X, Original.X, 0.01f);
		TestNearlyEqual(TEXT("Round-trip Y"), RoundTrip.Y, Original.Y, 0.01f);
		TestNearlyEqual(TEXT("Round-trip Z"), RoundTrip.Z, Original.Z, 0.01f);
	}

	// Test 6: Unit conversion (cm to m)
	{
		FVector UE5Pos(1000, 2000, 3000); // 10m, 20m, 30m
		FVector ColmapPos = FCoordinateConverter::ConvertPositionToColmap(UE5Pos);

		// Check that values are in meters (original / 100)
		TestTrue(TEXT("X in meters range"), FMath::Abs(ColmapPos.X) <= 30.0);
		TestTrue(TEXT("Y in meters range"), FMath::Abs(ColmapPos.Y) <= 30.0);
		TestTrue(TEXT("Z in meters range"), FMath::Abs(ColmapPos.Z) <= 30.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoordinateConverterRotationTest, "UE5_3DGS.FCM.CoordinateConverter.Rotation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoordinateConverterRotationTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Test 1: Identity rotation
	{
		FRotator UE5Identity(0, 0, 0);
		FQuat ColmapQuat = FCoordinateConverter::ConvertRotationToColmap(UE5Identity);

		// Quaternion should be normalized
		float QuatLength = ColmapQuat.Size();
		TestNearlyEqual(TEXT("Identity quat is unit"), QuatLength, 1.0f, 0.0001f);
	}

	// Test 2: 90-degree yaw rotation
	{
		FRotator UE5Yaw90(0, 90, 0);
		FQuat ColmapQuat = FCoordinateConverter::ConvertRotationToColmap(UE5Yaw90);

		// Should be valid quaternion
		TestTrue(TEXT("Yaw90 quat is valid"), ColmapQuat.IsNormalized());
	}

	// Test 3: Round-trip rotation conversion
	{
		FRotator Original(15.0f, 45.0f, -30.0f);
		FQuat ColmapQuat = FCoordinateConverter::ConvertRotationToColmap(Original);
		FRotator RoundTrip = FCoordinateConverter::ConvertRotationFromColmap(ColmapQuat);

		// Note: Rotations may differ by 360 degrees
		float PitchDiff = FMath::Abs(FMath::Fmod(RoundTrip.Pitch - Original.Pitch + 180.0f, 360.0f) - 180.0f);
		float YawDiff = FMath::Abs(FMath::Fmod(RoundTrip.Yaw - Original.Yaw + 180.0f, 360.0f) - 180.0f);
		float RollDiff = FMath::Abs(FMath::Fmod(RoundTrip.Roll - Original.Roll + 180.0f, 360.0f) - 180.0f);

		TestTrue(TEXT("Round-trip Pitch"), PitchDiff < 1.0f);
		TestTrue(TEXT("Round-trip Yaw"), YawDiff < 1.0f);
		TestTrue(TEXT("Round-trip Roll"), RollDiff < 1.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraIntrinsicsTest, "UE5_3DGS.FCM.CameraIntrinsics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCameraIntrinsicsTest::RunTest(const FString& Parameters)
{
	// Test 1: FOV to focal length conversion
	{
		double FOV = 90.0;
		double Width = 1920.0;
		double FocalLength = FCameraIntrinsics::ComputeFocalLengthFromFOV(FOV, Width);

		// At 90 degree FOV, focal length should equal half the width
		TestNearlyEqual(TEXT("90 FOV focal length"), FocalLength, Width / 2.0, 1.0);
	}

	// Test 2: Focal length to FOV conversion (inverse)
	{
		double Width = 1920.0;
		double FocalLength = 960.0; // Width / 2
		double FOV = FCameraIntrinsics::ComputeFOVFromFocalLength(FocalLength, Width);

		TestNearlyEqual(TEXT("Focal to FOV"), FOV, 90.0, 0.1);
	}

	// Test 3: Round-trip FOV conversion
	{
		double OriginalFOV = 75.0;
		double Width = 1920.0;
		double FocalLength = FCameraIntrinsics::ComputeFocalLengthFromFOV(OriginalFOV, Width);
		double RecoveredFOV = FCameraIntrinsics::ComputeFOVFromFocalLength(FocalLength, Width);

		TestNearlyEqual(TEXT("Round-trip FOV"), RecoveredFOV, OriginalFOV, 0.001);
	}

	// Test 4: Intrinsics validity check
	{
		FCameraIntrinsics ValidIntrinsics(1920, 1080, 90.0f);
		TestTrue(TEXT("Valid intrinsics"), ValidIntrinsics.IsValid());

		FCameraIntrinsics InvalidIntrinsics;
		InvalidIntrinsics.Width = 0;
		TestFalse(TEXT("Invalid intrinsics (zero width)"), InvalidIntrinsics.IsValid());
	}

	// Test 5: COLMAP parameter string
	{
		FCameraIntrinsics Intrinsics(1920, 1080, 90.0f);
		Intrinsics.CameraModel = EColmapCameraModel::PINHOLE;
		FString ParamString = Intrinsics.GetColmapParamsString();

		TestTrue(TEXT("Param string has values"), ParamString.Len() > 0);
		TestTrue(TEXT("Param string contains focal"), ParamString.Contains(TEXT(".")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrajectoryGeneratorTest, "UE5_3DGS.SCM.TrajectoryGenerator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrajectoryGeneratorTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Test 1: Orbital trajectory generation
	{
		FTrajectoryConfig Config;
		Config.TrajectoryType = ECameraTrajectoryType::Orbital;
		Config.NumRings = 3;
		Config.ViewsPerRing = 12;
		Config.BaseRadius = 500.0f;
		Config.FocusPoint = FVector::ZeroVector;

		TArray<FCameraViewpoint> Viewpoints = FCameraTrajectoryGenerator::GenerateViewpoints(Config);

		int32 ExpectedCount = Config.NumRings * Config.ViewsPerRing;
		TestEqual(TEXT("Orbital viewpoint count"), Viewpoints.Num(), ExpectedCount);
	}

	// Test 2: Viewpoints look at focus point
	{
		FTrajectoryConfig Config;
		Config.TrajectoryType = ECameraTrajectoryType::Orbital;
		Config.NumRings = 1;
		Config.ViewsPerRing = 4;
		Config.BaseRadius = 500.0f;
		Config.FocusPoint = FVector(100, 100, 100);
		Config.bLookAtFocusPoint = true;

		TArray<FCameraViewpoint> Viewpoints = FCameraTrajectoryGenerator::GenerateViewpoints(Config);

		for (const FCameraViewpoint& VP : Viewpoints)
		{
			// Check that camera is looking roughly toward focus point
			FVector LookDir = VP.Rotation.Vector();
			FVector ToFocus = (Config.FocusPoint - VP.Position).GetSafeNormal();
			float Dot = FVector::DotProduct(LookDir, ToFocus);

			TestTrue(TEXT("Camera looks at focus"), Dot > 0.9f);
		}
	}

	// Test 3: Viewpoints are at correct distance
	{
		FTrajectoryConfig Config;
		Config.TrajectoryType = ECameraTrajectoryType::Orbital;
		Config.NumRings = 2;
		Config.ViewsPerRing = 8;
		Config.BaseRadius = 1000.0f;
		Config.FocusPoint = FVector::ZeroVector;
		Config.bVaryRadiusPerRing = false;

		TArray<FCameraViewpoint> Viewpoints = FCameraTrajectoryGenerator::GenerateViewpoints(Config);

		for (const FCameraViewpoint& VP : Viewpoints)
		{
			float Distance = FVector::Distance(VP.Position, Config.FocusPoint);
			TestNearlyEqual(TEXT("Viewpoint at correct distance"), Distance, Config.BaseRadius, 10.0f);
		}
	}

	// Test 4: Optimal config calculation
	{
		FBox BoundingBox(FVector(-500, -500, 0), FVector(500, 500, 300));
		FTrajectoryConfig OptimalConfig = FCameraTrajectoryGenerator::CalculateOptimalConfig(BoundingBox);

		TestTrue(TEXT("Optimal config has positive radius"), OptimalConfig.BaseRadius > 0);
		TestTrue(TEXT("Optimal config has multiple rings"), OptimalConfig.NumRings >= 2);
		TestTrue(TEXT("Optimal config has multiple views"), OptimalConfig.ViewsPerRing >= 8);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FColmapWriterTest, "UE5_3DGS.FCM.ColmapWriter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FColmapWriterTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Test 1: Image filename generation
	{
		FCameraIntrinsics Intrinsics(1920, 1080, 90.0f);

		TArray<FCameraViewpoint> Viewpoints;
		FCameraViewpoint VP1;
		VP1.Position = FVector(0, 0, 100);
		VP1.Rotation = FRotator(0, 0, 0);
		VP1.ViewpointId = 0;
		Viewpoints.Add(VP1);

		TArray<FColmapImage> Images = FColmapWriter::CreateImagesFromViewpoints(Viewpoints, Intrinsics);

		TestEqual(TEXT("Image count matches"), Images.Num(), 1);
		TestTrue(TEXT("Image name has prefix"), Images[0].ImageName.StartsWith(TEXT("image_")));
		TestTrue(TEXT("Image name has extension"), Images[0].ImageName.Contains(TEXT(".")));
	}

	// Test 2: Camera model ID mapping
	{
		FCameraIntrinsics Intrinsics;

		Intrinsics.CameraModel = EColmapCameraModel::SIMPLE_PINHOLE;
		TestEqual(TEXT("SIMPLE_PINHOLE ID"), Intrinsics.GetColmapModelId(), 0);

		Intrinsics.CameraModel = EColmapCameraModel::PINHOLE;
		TestEqual(TEXT("PINHOLE ID"), Intrinsics.GetColmapModelId(), 1);

		Intrinsics.CameraModel = EColmapCameraModel::OPENCV;
		TestEqual(TEXT("OPENCV ID"), Intrinsics.GetColmapModelId(), 4);
	}

	// Test 3: Quaternion conversion in images
	{
		FCameraIntrinsics Intrinsics(1920, 1080, 90.0f);

		TArray<FCameraViewpoint> Viewpoints;
		FCameraViewpoint VP;
		VP.Position = FVector(500, 0, 0);
		VP.Rotation = FRotator(0, 0, 0);
		VP.ViewpointId = 0;
		Viewpoints.Add(VP);

		TArray<FColmapImage> Images = FColmapWriter::CreateImagesFromViewpoints(Viewpoints, Intrinsics);

		// Quaternion should be normalized
		float QuatSize = Images[0].Rotation.Size();
		TestNearlyEqual(TEXT("Image rotation normalized"), QuatSize, 1.0f, 0.001f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlyWriterTest, "UE5_3DGS.FCM.PlyWriter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlyWriterTest::RunTest(const FString& Parameters)
{
	using namespace UE5_3DGS;

	// Test 1: SH coefficient conversion
	{
		FColor White(255, 255, 255, 255);
		FVector SH_DC = FGaussianSplat::ColorToSH_DC(White);

		// White should produce positive SH values
		TestTrue(TEXT("White SH_DC.X > 0"), SH_DC.X > 0);
		TestTrue(TEXT("White SH_DC.Y > 0"), SH_DC.Y > 0);
		TestTrue(TEXT("White SH_DC.Z > 0"), SH_DC.Z > 0);
	}

	// Test 2: SH round-trip conversion
	{
		FColor Original(128, 64, 192, 255);
		FVector SH_DC = FGaussianSplat::ColorToSH_DC(Original);
		FColor Recovered = FGaussianSplat::SH_DCToColor(SH_DC);

		// Should be within 1 value due to rounding
		TestTrue(TEXT("SH round-trip R"), FMath::Abs(Recovered.R - Original.R) <= 1);
		TestTrue(TEXT("SH round-trip G"), FMath::Abs(Recovered.G - Original.G) <= 1);
		TestTrue(TEXT("SH round-trip B"), FMath::Abs(Recovered.B - Original.B) <= 1);
	}

	// Test 3: Memory estimation
	{
		int64 Memory100K = FPlyWriter::EstimateMemoryUsage(100000);
		int64 Memory1M = FPlyWriter::EstimateMemoryUsage(1000000);

		// 236 bytes per splat
		TestEqual(TEXT("100K splats memory"), Memory100K, 100000LL * 236);
		TestEqual(TEXT("1M splats memory"), Memory1M, 1000000LL * 236);
	}

	// Test 4: Point cloud creation
	{
		TArray<FVector> Vertices = { FVector(0,0,0), FVector(100,0,0), FVector(0,100,0) };
		TArray<FVector> Normals = { FVector(0,0,1), FVector(0,0,1), FVector(0,0,1) };
		TArray<FColor> Colors = { FColor::Red, FColor::Green, FColor::Blue };

		TArray<FPointCloudPoint> Points = FPlyWriter::CreatePointCloudFromMesh(Vertices, Normals, Colors);

		TestEqual(TEXT("Point cloud count"), Points.Num(), 3);
		TestEqual(TEXT("Point 0 color"), Points[0].Color.R, (uint8)255);
	}

	// Test 5: Splat validation
	{
		TArray<FGaussianSplat> ValidSplats;
		for (int32 i = 0; i < 100; ++i)
		{
			FGaussianSplat Splat;
			Splat.Position = FVector(i * 10, 0, 0);
			Splat.Opacity = 1.0f;
			Splat.Scale = FVector(-5.0f, -5.0f, -5.0f);
			ValidSplats.Add(Splat);
		}

		TArray<FString> Warnings;
		bool bValid = FPlyWriter::ValidateSplats(ValidSplats, Warnings);

		TestTrue(TEXT("Valid splats pass validation"), bValid);
	}

	return true;
}
