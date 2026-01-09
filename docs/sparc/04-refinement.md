# SPARC Refinement Document
## UE5-3DGS: TDD Anchors and Test Specifications

**Version:** 1.0
**Phase:** Refinement
**Status:** Draft

---

## 1. Test-Driven Development Strategy

### 1.1 TDD Workflow

```
RED -> GREEN -> REFACTOR

1. Write failing test (RED)
2. Write minimal code to pass (GREEN)
3. Refactor for quality (REFACTOR)
4. Repeat
```

### 1.2 Test Categories

| Category | Coverage Target | Execution Time | Frequency |
|----------|-----------------|----------------|-----------|
| Unit Tests | 90% | < 100ms each | Every build |
| Integration Tests | 80% | < 5s each | Pre-commit |
| System Tests | 70% | < 60s each | Nightly |
| Performance Tests | Critical paths | Varies | Weekly |

---

## 2. Unit Test Specifications

### 2.1 Coordinate Conversion Tests

#### 2.1.1 UE5 to COLMAP Position Conversion

```cpp
// Test: Position conversion from UE5 (cm, left-handed) to COLMAP (m, right-handed)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCoordinateConversion_Position_Test,
    "UE5_3DGS.DEM.CoordinateConversion.Position",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCoordinateConversion_Position_Test::RunTest(const FString& Parameters)
{
    FDEM_CoordinateConverter Converter;

    // Test Case 1: Origin
    {
        FVector UE5Pos(0, 0, 0);
        FVector COLMAPPos = Converter.ConvertPositionToCOLMAP(UE5Pos);
        TestEqual("Origin X", COLMAPPos.X, 0.0f, 0.0001f);
        TestEqual("Origin Y", COLMAPPos.Y, 0.0f, 0.0001f);
        TestEqual("Origin Z", COLMAPPos.Z, 0.0f, 0.0001f);
    }

    // Test Case 2: Unit vectors
    {
        // UE5: X=100cm forward -> COLMAP: Z=1m forward
        FVector UE5Forward(100, 0, 0);
        FVector COLMAPForward = Converter.ConvertPositionToCOLMAP(UE5Forward);
        TestEqual("Forward maps to Z", COLMAPForward.Z, 1.0f, 0.0001f);
        TestEqual("Forward X is zero", COLMAPForward.X, 0.0f, 0.0001f);
        TestEqual("Forward Y is zero", COLMAPForward.Y, 0.0f, 0.0001f);

        // UE5: Y=100cm right -> COLMAP: X=1m right
        FVector UE5Right(0, 100, 0);
        FVector COLMAPRight = Converter.ConvertPositionToCOLMAP(UE5Right);
        TestEqual("Right maps to X", COLMAPRight.X, 1.0f, 0.0001f);

        // UE5: Z=100cm up -> COLMAP: Y=-1m (down in COLMAP = negative Y)
        FVector UE5Up(0, 0, 100);
        FVector COLMAPUp = Converter.ConvertPositionToCOLMAP(UE5Up);
        TestEqual("Up maps to -Y", COLMAPUp.Y, -1.0f, 0.0001f);
    }

    // Test Case 3: Arbitrary position
    {
        FVector UE5Pos(500, 300, 200);  // 5m forward, 3m right, 2m up
        FVector COLMAPPos = Converter.ConvertPositionToCOLMAP(UE5Pos);
        TestEqual("Arbitrary X (from UE5.Y)", COLMAPPos.X, 3.0f, 0.0001f);
        TestEqual("Arbitrary Y (from -UE5.Z)", COLMAPPos.Y, -2.0f, 0.0001f);
        TestEqual("Arbitrary Z (from UE5.X)", COLMAPPos.Z, 5.0f, 0.0001f);
    }

    return true;
}
```

#### 2.1.2 UE5 to COLMAP Rotation Conversion

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCoordinateConversion_Rotation_Test,
    "UE5_3DGS.DEM.CoordinateConversion.Rotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCoordinateConversion_Rotation_Test::RunTest(const FString& Parameters)
{
    FDEM_CoordinateConverter Converter;

    // Test Case 1: Identity rotation (camera looking forward in UE5)
    {
        FQuat UE5Identity = FQuat::Identity;
        FCOLMAPExtrinsics Result = Converter.ConvertToCOLMAP(FTransform(UE5Identity, FVector::ZeroVector));

        // COLMAP identity should have camera looking along +Z
        // Verify quaternion represents identity-like rotation
        TestTrue("Identity quaternion W near 1", FMath::Abs(Result.Quaternion.W) > 0.9f);
    }

    // Test Case 2: 90-degree yaw (camera looking right in UE5)
    {
        FQuat UE5Yaw90 = FQuat(FVector::UpVector, FMath::DegreesToRadians(90));
        FCOLMAPExtrinsics Result = Converter.ConvertToCOLMAP(FTransform(UE5Yaw90, FVector::ZeroVector));

        // Verify the rotation is preserved (magnitude check)
        float Angle = 2.0f * FMath::Acos(FMath::Abs(Result.Quaternion.W));
        TestTrue("90 degree rotation preserved", FMath::Abs(FMath::RadiansToDegrees(Angle) - 90.0f) < 1.0f);
    }

    // Test Case 3: Round-trip conversion
    {
        FQuat OriginalRotation = FQuat(FRotator(15, 30, 45));
        FTransform OriginalTransform(OriginalRotation, FVector(100, 200, 300));

        FCOLMAPExtrinsics COLMAPData = Converter.ConvertToCOLMAP(OriginalTransform);
        FTransform RoundTripped = Converter.ConvertFromCOLMAP(COLMAPData);

        // Position round-trip
        TestTrue("Position X preserved",
            FMath::IsNearlyEqual(OriginalTransform.GetLocation().X, RoundTripped.GetLocation().X, 0.1f));
        TestTrue("Position Y preserved",
            FMath::IsNearlyEqual(OriginalTransform.GetLocation().Y, RoundTripped.GetLocation().Y, 0.1f));
        TestTrue("Position Z preserved",
            FMath::IsNearlyEqual(OriginalTransform.GetLocation().Z, RoundTripped.GetLocation().Z, 0.1f));

        // Rotation round-trip (quaternion comparison allowing for sign flip)
        FQuat OrigQ = OriginalTransform.GetRotation();
        FQuat RoundQ = RoundTripped.GetRotation();
        float QuatDot = FMath::Abs(OrigQ | RoundQ);
        TestTrue("Rotation preserved", QuatDot > 0.9999f);
    }

    return true;
}
```

#### 2.1.3 Camera Intrinsic Matrix Tests

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCameraIntrinsics_Test,
    "UE5_3DGS.DEM.CameraIntrinsics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCameraIntrinsics_Test::RunTest(const FString& Parameters)
{
    // Test Case 1: Standard 90-degree horizontal FOV
    {
        float HFOV = 90.0f;
        int32 Width = 1920;
        int32 Height = 1080;

        FCameraIntrinsics Intrinsics = FDEM_CameraParameters::ComputeIntrinsics(HFOV, Width, Height);

        // For 90-degree FOV: fx = Width / (2 * tan(45)) = Width / 2
        float ExpectedFocal = Width / 2.0f;
        TestEqual("Focal length for 90 FOV", Intrinsics.FocalX, ExpectedFocal, 0.01f);
        TestEqual("Focal X equals Focal Y", Intrinsics.FocalX, Intrinsics.FocalY, 0.0001f);

        // Principal point at center
        TestEqual("Principal X at center", Intrinsics.PrincipalX, Width / 2.0f, 0.01f);
        TestEqual("Principal Y at center", Intrinsics.PrincipalY, Height / 2.0f, 0.01f);
    }

    // Test Case 2: Narrower 60-degree FOV
    {
        float HFOV = 60.0f;
        int32 Width = 1920;
        int32 Height = 1080;

        FCameraIntrinsics Intrinsics = FDEM_CameraParameters::ComputeIntrinsics(HFOV, Width, Height);

        // For 60-degree FOV: fx = Width / (2 * tan(30)) = Width / (2 * 0.577) ~ Width * 0.866
        float ExpectedFocal = Width / (2.0f * FMath::Tan(FMath::DegreesToRadians(30.0f)));
        TestEqual("Focal length for 60 FOV", Intrinsics.FocalX, ExpectedFocal, 0.1f);
    }

    // Test Case 3: Reprojection verification
    {
        float HFOV = 90.0f;
        int32 Width = 1920;
        int32 Height = 1080;

        FCameraIntrinsics Intrinsics = FDEM_CameraParameters::ComputeIntrinsics(HFOV, Width, Height);

        // A point at (1, 0, 1) in camera space should project to right edge
        FVector CameraPoint(1.0f, 0.0f, 1.0f);  // 45 degrees right
        float ProjectedX = Intrinsics.FocalX * (CameraPoint.X / CameraPoint.Z) + Intrinsics.PrincipalX;
        float ProjectedY = Intrinsics.FocalY * (CameraPoint.Y / CameraPoint.Z) + Intrinsics.PrincipalY;

        TestEqual("Point at 45 deg projects to right edge", ProjectedX, (float)Width, 1.0f);
        TestEqual("Point at center Y projects to center", ProjectedY, Height / 2.0f, 0.01f);
    }

    return true;
}
```

### 2.2 Depth Conversion Tests

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDepthConversion_Test,
    "UE5_3DGS.DEM.DepthConversion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FDepthConversion_Test::RunTest(const FString& Parameters)
{
    FDEM_DepthExtractor Extractor;
    Extractor.SetClipPlanes(10.0f, 100000.0f);  // 10cm to 1000m

    // Test Case 1: Near plane depth
    {
        float ReverseZ = 1.0f;  // Near plane in reverse-Z
        float LinearDepth = Extractor.ConvertReverseZToLinear(ReverseZ);
        TestEqual("Near plane (1.0) converts to near clip", LinearDepth, 0.1f, 0.001f);  // 10cm = 0.1m
    }

    // Test Case 2: Mid-range depth
    {
        // For reverse-Z: Z_buffer = Far / (Far - Z_eye) for infinite far
        // For finite: Z_buffer = (Far * Near) / (Far - Z_eye * (Far - Near))
        float TestDepth = 1000.0f;  // 10 metres in cm
        float ReverseZ = (100000.0f * 10.0f) / (100000.0f - TestDepth * (100000.0f - 10.0f));

        float LinearDepth = Extractor.ConvertReverseZToLinear(ReverseZ);
        TestEqual("10m depth round-trip", LinearDepth, 10.0f, 0.01f);  // Should be 10 metres
    }

    // Test Case 3: Far plane depth
    {
        float ReverseZ = 0.0f;  // Far plane in reverse-Z
        float LinearDepth = Extractor.ConvertReverseZToLinear(ReverseZ);
        TestEqual("Far plane (0.0) converts to far clip", LinearDepth, 1000.0f, 1.0f);  // 1000m
    }

    // Test Case 4: Monotonicity
    {
        TArray<float> ReverseZValues = {1.0f, 0.8f, 0.5f, 0.2f, 0.0f};
        float PreviousDepth = 0.0f;

        for (float RZ : ReverseZValues)
        {
            float LinearDepth = Extractor.ConvertReverseZToLinear(RZ);
            TestTrue("Depth monotonically increasing", LinearDepth >= PreviousDepth);
            PreviousDepth = LinearDepth;
        }
    }

    return true;
}
```

### 2.3 Trajectory Generation Tests

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSphericalTrajectory_Test,
    "UE5_3DGS.SCM.SphericalTrajectory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FSphericalTrajectory_Test::RunTest(const FString& Parameters)
{
    FSCM_SphericalTrajectory Generator;

    // Test Case 1: Correct number of positions
    {
        FTrajectoryParams Params;
        Params.NumPositions = 100;
        Params.SphereCenter = FVector::ZeroVector;
        Params.SphereRadius = 500.0f;

        TArray<FTransform> Transforms = Generator.Generate(Params);
        TestEqual("Correct number of transforms", Transforms.Num(), 100);
    }

    // Test Case 2: All positions on sphere surface
    {
        FTrajectoryParams Params;
        Params.NumPositions = 50;
        Params.SphereCenter = FVector(100, 200, 300);
        Params.SphereRadius = 1000.0f;

        TArray<FTransform> Transforms = Generator.Generate(Params);

        for (int32 i = 0; i < Transforms.Num(); i++)
        {
            FVector Position = Transforms[i].GetLocation();
            float Distance = FVector::Distance(Position, Params.SphereCenter);
            TestEqual(FString::Printf(TEXT("Position %d on sphere"), i),
                Distance, Params.SphereRadius, 1.0f);
        }
    }

    // Test Case 3: All cameras look toward center
    {
        FTrajectoryParams Params;
        Params.NumPositions = 20;
        Params.SphereCenter = FVector::ZeroVector;
        Params.SphereRadius = 500.0f;

        TArray<FTransform> Transforms = Generator.Generate(Params);

        for (int32 i = 0; i < Transforms.Num(); i++)
        {
            FVector Position = Transforms[i].GetLocation();
            FVector Forward = Transforms[i].GetRotation().GetForwardVector();
            FVector ToCenter = (Params.SphereCenter - Position).GetSafeNormal();

            float Dot = FVector::DotProduct(Forward, ToCenter);
            TestTrue(FString::Printf(TEXT("Camera %d looks toward center"), i),
                Dot > 0.99f);
        }
    }

    // Test Case 4: Uniform distribution (no clustering)
    {
        FTrajectoryParams Params;
        Params.NumPositions = 100;
        Params.SphereCenter = FVector::ZeroVector;
        Params.SphereRadius = 1000.0f;

        TArray<FTransform> Transforms = Generator.Generate(Params);

        // Minimum expected distance between neighbors (Fibonacci lattice property)
        float ExpectedMinDist = 2.0f * PI * Params.SphereRadius / FMath::Sqrt((float)Params.NumPositions);
        float MinFoundDist = MAX_FLT;

        for (int32 i = 0; i < Transforms.Num(); i++)
        {
            for (int32 j = i + 1; j < Transforms.Num(); j++)
            {
                float Dist = FVector::Distance(
                    Transforms[i].GetLocation(),
                    Transforms[j].GetLocation()
                );
                MinFoundDist = FMath::Min(MinFoundDist, Dist);
            }
        }

        // Allow 20% tolerance
        TestTrue("No camera clustering",
            MinFoundDist > ExpectedMinDist * 0.8f);
    }

    return true;
}
```

### 2.4 COLMAP Writer Tests

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCOLMAPWriter_Cameras_Test,
    "UE5_3DGS.FCM.COLMAPWriter.Cameras",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCOLMAPWriter_Cameras_Test::RunTest(const FString& Parameters)
{
    FFCM_COLMAPWriter Writer;

    // Test Case 1: Write single camera
    {
        TArray<FCOLMAPCamera> Cameras;
        FCOLMAPCamera Camera;
        Camera.CameraID = 1;
        Camera.Model = ECameraModel::PINHOLE;
        Camera.Width = 1920;
        Camera.Height = 1080;
        Camera.FocalX = 1597.23f;
        Camera.FocalY = 1597.23f;
        Camera.PrincipalX = 960.0f;
        Camera.PrincipalY = 540.0f;
        Cameras.Add(Camera);

        FString TempPath = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("cameras"), TEXT(".txt"));

        bool bSuccess = Writer.WriteCamerasText(TempPath, Cameras);
        TestTrue("Camera file written", bSuccess);

        // Verify file contents
        FString FileContents;
        FFileHelper::LoadFileToString(FileContents, *TempPath);

        TestTrue("Contains PINHOLE", FileContents.Contains(TEXT("PINHOLE")));
        TestTrue("Contains resolution", FileContents.Contains(TEXT("1920")));
        TestTrue("Contains focal length", FileContents.Contains(TEXT("1597")));

        // Cleanup
        IFileManager::Get().Delete(*TempPath);
    }

    // Test Case 2: Write multiple cameras
    {
        TArray<FCOLMAPCamera> Cameras;
        for (int32 i = 1; i <= 5; i++)
        {
            FCOLMAPCamera Camera;
            Camera.CameraID = i;
            Camera.Model = ECameraModel::PINHOLE;
            Camera.Width = 1920;
            Camera.Height = 1080;
            Camera.FocalX = 1597.23f;
            Camera.FocalY = 1597.23f;
            Camera.PrincipalX = 960.0f;
            Camera.PrincipalY = 540.0f;
            Cameras.Add(Camera);
        }

        FString TempPath = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("cameras"), TEXT(".txt"));

        bool bSuccess = Writer.WriteCamerasText(TempPath, Cameras);
        TestTrue("Multiple cameras written", bSuccess);

        // Count non-comment lines
        TArray<FString> Lines;
        FFileHelper::LoadFileToStringArray(Lines, *TempPath);

        int32 DataLines = 0;
        for (const FString& Line : Lines)
        {
            if (!Line.StartsWith(TEXT("#")) && !Line.IsEmpty())
            {
                DataLines++;
            }
        }

        TestEqual("Correct number of camera entries", DataLines, 5);

        // Cleanup
        IFileManager::Get().Delete(*TempPath);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCOLMAPWriter_Images_Test,
    "UE5_3DGS.FCM.COLMAPWriter.Images",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCOLMAPWriter_Images_Test::RunTest(const FString& Parameters)
{
    FFCM_COLMAPWriter Writer;

    // Test Case 1: Quaternion format verification
    {
        TArray<FCOLMAPImage> Images;
        FCOLMAPImage Image;
        Image.ImageID = 1;
        Image.Quaternion = FQuat(0.5f, 0.5f, 0.5f, 0.5f);  // W, X, Y, Z
        Image.Translation = FVector(1.0f, 2.0f, 3.0f);
        Image.CameraID = 1;
        Image.Name = TEXT("frame_00000.png");
        Images.Add(Image);

        FString TempPath = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("images"), TEXT(".txt"));

        bool bSuccess = Writer.WriteImagesText(TempPath, Images);
        TestTrue("Images file written", bSuccess);

        // Verify quaternion order (W first in COLMAP)
        FString FileContents;
        FFileHelper::LoadFileToString(FileContents, *TempPath);

        // Should see: IMAGE_ID QW QX QY QZ TX TY TZ CAMERA_ID NAME
        // Quaternion: 0.5 0.5 0.5 0.5
        TestTrue("Contains quaternion W first", FileContents.Contains(TEXT("1 0.5")));
        TestTrue("Contains image name", FileContents.Contains(TEXT("frame_00000.png")));

        // Cleanup
        IFileManager::Get().Delete(*TempPath);
    }

    // Test Case 2: Binary format round-trip
    {
        TArray<FCOLMAPImage> OriginalImages;
        for (int32 i = 0; i < 10; i++)
        {
            FCOLMAPImage Image;
            Image.ImageID = i + 1;
            Image.Quaternion = FQuat(FRotator(i * 10.0f, i * 20.0f, i * 5.0f));
            Image.Translation = FVector(i * 100.0f, i * 50.0f, i * 25.0f);
            Image.CameraID = 1;
            Image.Name = FString::Printf(TEXT("frame_%05d.png"), i);
            OriginalImages.Add(Image);
        }

        FString TempBinaryPath = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("images"), TEXT(".bin"));

        bool bWriteSuccess = Writer.WriteImagesBinary(TempBinaryPath, OriginalImages);
        TestTrue("Binary images written", bWriteSuccess);

        // Read back and verify
        TArray<FCOLMAPImage> ReadImages;
        bool bReadSuccess = Writer.ReadImagesBinary(TempBinaryPath, ReadImages);
        TestTrue("Binary images read", bReadSuccess);

        TestEqual("Same number of images", ReadImages.Num(), OriginalImages.Num());

        for (int32 i = 0; i < OriginalImages.Num(); i++)
        {
            TestEqual(FString::Printf(TEXT("Image %d ID"), i),
                ReadImages[i].ImageID, OriginalImages[i].ImageID);
            TestTrue(FString::Printf(TEXT("Image %d quaternion"), i),
                ReadImages[i].Quaternion.Equals(OriginalImages[i].Quaternion, 0.0001f));
            TestTrue(FString::Printf(TEXT("Image %d translation"), i),
                ReadImages[i].Translation.Equals(OriginalImages[i].Translation, 0.001f));
        }

        // Cleanup
        IFileManager::Get().Delete(*TempBinaryPath);
    }

    return true;
}
```

---

## 3. Integration Test Specifications

### 3.1 Full Pipeline Integration Test

```cpp
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FFullPipeline_Integration_Test,
    "UE5_3DGS.Integration.FullPipeline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::HighPriority
)

void FFullPipeline_Integration_Test::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands
) const
{
    OutBeautifiedNames.Add("BasicScene_100Frames");
    OutTestCommands.Add("/Game/3DGS_Test/TestLevel_Basic");

    OutBeautifiedNames.Add("ComplexScene_500Frames");
    OutTestCommands.Add("/Game/3DGS_Test/TestLevel_Complex");
}

bool FFullPipeline_Integration_Test::RunTest(const FString& Parameters)
{
    // Parse test parameters
    FString LevelPath = Parameters;
    int32 NumFrames = LevelPath.Contains(TEXT("Basic")) ? 100 : 500;

    // Step 1: Load test level
    UWorld* TestWorld = nullptr;
    {
        ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand(LevelPath));
        ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());
        ADD_LATENT_AUTOMATION_COMMAND(FGetWorldCommand(&TestWorld));
    }

    // Step 2: Configure capture
    FCaptureConfig Config;
    Config.Resolution = FIntPoint(1280, 720);
    Config.NumFrames = NumFrames;
    Config.TrajectoryType = ESCM_TrajectoryType::Spherical;
    Config.ExportFormat = EFCM_ExportFormat::COLMAP_Binary;
    Config.OutputPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Test_Export"));

    // Step 3: Execute capture
    TSharedPtr<FSCM_CaptureOrchestrator> Orchestrator = MakeShared<FSCM_CaptureOrchestrator>();

    bool bCaptureSuccess = false;
    TFuture<FCaptureResult> CaptureFuture = Orchestrator->ExecuteCaptureAsync(TestWorld, Config);

    // Wait for completion with timeout
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForFutureCommand(CaptureFuture, 300.0f)); // 5 minute timeout

    FCaptureResult Result = CaptureFuture.Get();
    bCaptureSuccess = Result.bSuccess;

    TestTrue("Capture completed successfully", bCaptureSuccess);

    // Step 4: Validate output files
    {
        FString OutputPath = Config.OutputPath;

        // Check images directory
        TArray<FString> ImageFiles;
        IFileManager::Get().FindFiles(ImageFiles, *FPaths::Combine(OutputPath, TEXT("images")), TEXT("*.png"));
        TestEqual("Correct number of images", ImageFiles.Num(), NumFrames);

        // Check COLMAP files
        FString SparseDir = FPaths::Combine(OutputPath, TEXT("sparse"), TEXT("0"));
        TestTrue("cameras.bin exists",
            IFileManager::Get().FileExists(*FPaths::Combine(SparseDir, TEXT("cameras.bin"))));
        TestTrue("images.bin exists",
            IFileManager::Get().FileExists(*FPaths::Combine(SparseDir, TEXT("images.bin"))));
        TestTrue("points3D.bin exists",
            IFileManager::Get().FileExists(*FPaths::Combine(SparseDir, TEXT("points3D.bin"))));
    }

    // Step 5: Validate COLMAP format with external tool (if available)
    {
        FString ColmapPath;
        if (FPlatformProcess::SearchPathForExecutable(TEXT("colmap"), ColmapPath))
        {
            FString Args = FString::Printf(TEXT("model_analyzer --input_path \"%s\""),
                *FPaths::Combine(Config.OutputPath, TEXT("sparse"), TEXT("0")));

            FString StdOut, StdErr;
            int32 ReturnCode = -1;

            FPlatformProcess::ExecProcess(*ColmapPath, *Args, &ReturnCode, &StdOut, &StdErr);

            TestEqual("COLMAP validation passed", ReturnCode, 0);
        }
        else
        {
            AddWarning(TEXT("COLMAP not found in PATH, skipping external validation"));
        }
    }

    // Cleanup test data
    IFileManager::Get().DeleteDirectory(*Config.OutputPath, false, true);

    return true;
}
```

### 3.2 COLMAP Compatibility Integration Test

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCOLMAPCompatibility_Test,
    "UE5_3DGS.Integration.COLMAPCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCOLMAPCompatibility_Test::RunTest(const FString& Parameters)
{
    // Generate minimal test dataset
    FFCM_COLMAPWriter Writer;

    FString OutputPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("COLMAP_Test"));
    FString SparseDir = FPaths::Combine(OutputPath, TEXT("sparse"), TEXT("0"));
    FString ImagesDir = FPaths::Combine(OutputPath, TEXT("images"));

    // Create directories
    IFileManager::Get().MakeDirectory(*SparseDir, true);
    IFileManager::Get().MakeDirectory(*ImagesDir, true);

    // Create test camera
    TArray<FCOLMAPCamera> Cameras;
    FCOLMAPCamera Camera;
    Camera.CameraID = 1;
    Camera.Model = ECameraModel::PINHOLE;
    Camera.Width = 640;
    Camera.Height = 480;
    Camera.FocalX = 525.0f;
    Camera.FocalY = 525.0f;
    Camera.PrincipalX = 320.0f;
    Camera.PrincipalY = 240.0f;
    Cameras.Add(Camera);

    // Create test images
    TArray<FCOLMAPImage> Images;
    for (int32 i = 0; i < 10; i++)
    {
        FCOLMAPImage Image;
        Image.ImageID = i + 1;
        Image.Quaternion = FQuat::Identity;
        Image.Translation = FVector(i * 0.1f, 0, 0);
        Image.CameraID = 1;
        Image.Name = FString::Printf(TEXT("frame_%05d.png"), i);
        Images.Add(Image);

        // Create dummy image file
        TArray<uint8> DummyPNG;
        // ... Generate minimal valid PNG
        FFileHelper::SaveArrayToFile(DummyPNG,
            *FPaths::Combine(ImagesDir, Image.Name));
    }

    // Write COLMAP files (test both text and binary)
    bool bTextSuccess = Writer.WriteCamerasText(FPaths::Combine(SparseDir, TEXT("cameras.txt")), Cameras)
        && Writer.WriteImagesText(FPaths::Combine(SparseDir, TEXT("images.txt")), Images)
        && Writer.WritePoints3DText(FPaths::Combine(SparseDir, TEXT("points3D.txt")), TArray<FCOLMAPPoint3D>());

    bool bBinarySuccess = Writer.WriteCamerasBinary(FPaths::Combine(SparseDir, TEXT("cameras.bin")), Cameras)
        && Writer.WriteImagesBinary(FPaths::Combine(SparseDir, TEXT("images.bin")), Images)
        && Writer.WritePoints3DBinary(FPaths::Combine(SparseDir, TEXT("points3D.bin")), TArray<FCOLMAPPoint3D>());

    TestTrue("Text format written", bTextSuccess);
    TestTrue("Binary format written", bBinarySuccess);

    // Validate using internal parser
    FCOLMAPDataset TextDataset, BinaryDataset;
    bool bTextParsed = FCOLMAPParser::Parse(SparseDir, TEXT("txt"), TextDataset);
    bool bBinaryParsed = FCOLMAPParser::Parse(SparseDir, TEXT("bin"), BinaryDataset);

    TestTrue("Text format parseable", bTextParsed);
    TestTrue("Binary format parseable", bBinaryParsed);

    // Compare text and binary results
    TestEqual("Same camera count", TextDataset.Cameras.Num(), BinaryDataset.Cameras.Num());
    TestEqual("Same image count", TextDataset.Images.Num(), BinaryDataset.Images.Num());

    // Cleanup
    IFileManager::Get().DeleteDirectory(*OutputPath, false, true);

    return true;
}
```

---

## 4. Validation Metrics

### 4.1 Quality Metrics Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| Position Accuracy | < 1mm | Round-trip conversion test |
| Rotation Accuracy | < 0.01 degrees | Quaternion angle comparison |
| Depth Accuracy | < 0.1% relative error | Known distance validation |
| Intrinsic Accuracy | < 0.5 pixel | Reprojection error |
| Coverage | > 95% | Voxel coverage analysis |
| PSNR (reconstruction) | > 30 dB | Holdout view evaluation |
| SSIM (reconstruction) | > 0.95 | Structural similarity |

### 4.2 Performance Benchmarks

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPerformanceBenchmark_Test,
    "UE5_3DGS.Performance.Benchmarks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter
)

bool FPerformanceBenchmark_Test::RunTest(const FString& Parameters)
{
    // Benchmark 1: Trajectory generation
    {
        FSCM_SphericalTrajectory Generator;
        FTrajectoryParams Params;
        Params.NumPositions = 1000;
        Params.SphereCenter = FVector::ZeroVector;
        Params.SphereRadius = 1000.0f;

        double StartTime = FPlatformTime::Seconds();
        TArray<FTransform> Transforms = Generator.Generate(Params);
        double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

        TestTrue("Trajectory gen < 100ms", ElapsedMs < 100.0);
        AddInfo(FString::Printf(TEXT("Trajectory generation: %.2f ms"), ElapsedMs));
    }

    // Benchmark 2: Coordinate conversion (batch)
    {
        FDEM_CoordinateConverter Converter;
        TArray<FTransform> Transforms;
        Transforms.SetNum(1000);
        for (int32 i = 0; i < 1000; i++)
        {
            Transforms[i] = FTransform(
                FQuat(FRotator(FMath::FRand() * 360, FMath::FRand() * 360, FMath::FRand() * 360)),
                FVector(FMath::FRand() * 10000, FMath::FRand() * 10000, FMath::FRand() * 10000)
            );
        }

        double StartTime = FPlatformTime::Seconds();
        for (const FTransform& Transform : Transforms)
        {
            FCOLMAPExtrinsics Result = Converter.ConvertToCOLMAP(Transform);
        }
        double ElapsedUs = (FPlatformTime::Seconds() - StartTime) * 1000000.0;

        float PerConversionUs = ElapsedUs / 1000.0f;
        TestTrue("Coordinate conversion < 1us each", PerConversionUs < 1.0f);
        AddInfo(FString::Printf(TEXT("Per-conversion time: %.3f us"), PerConversionUs));
    }

    // Benchmark 3: COLMAP write (1000 images)
    {
        FFCM_COLMAPWriter Writer;

        TArray<FCOLMAPImage> Images;
        for (int32 i = 0; i < 1000; i++)
        {
            FCOLMAPImage Image;
            Image.ImageID = i + 1;
            Image.Quaternion = FQuat::Identity;
            Image.Translation = FVector(i, 0, 0);
            Image.CameraID = 1;
            Image.Name = FString::Printf(TEXT("frame_%05d.png"), i);
            Images.Add(Image);
        }

        FString TempPath = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("images"), TEXT(".bin"));

        double StartTime = FPlatformTime::Seconds();
        Writer.WriteImagesBinary(TempPath, Images);
        double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

        TestTrue("COLMAP write < 5s for 1000 images", ElapsedMs < 5000.0);
        AddInfo(FString::Printf(TEXT("COLMAP write (1000 images): %.2f ms"), ElapsedMs));

        IFileManager::Get().Delete(*TempPath);
    }

    return true;
}
```

### 4.3 Reconstruction Quality Test

```cpp
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FReconstructionQuality_Test,
    "UE5_3DGS.Quality.Reconstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::LowPriority
)

void FReconstructionQuality_Test::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands
) const
{
    OutBeautifiedNames.Add("SyntheticRoom_PSNR30");
    OutTestCommands.Add("/Game/3DGS_Test/QualityTest_Room:30.0:0.95");
}

bool FReconstructionQuality_Test::RunTest(const FString& Parameters)
{
    // Parse parameters: "LevelPath:TargetPSNR:TargetSSIM"
    TArray<FString> Parts;
    Parameters.ParseIntoArray(Parts, TEXT(":"));

    if (Parts.Num() < 3)
    {
        AddError(TEXT("Invalid test parameters"));
        return false;
    }

    FString LevelPath = Parts[0];
    float TargetPSNR = FCString::Atof(*Parts[1]);
    float TargetSSIM = FCString::Atof(*Parts[2]);

    // This test requires external 3DGS training
    // Check if gaussian-splatting is available
    FString GaussianSplattingPath = TEXT("/path/to/gaussian-splatting");
    if (!IFileManager::Get().DirectoryExists(*GaussianSplattingPath))
    {
        AddWarning(TEXT("gaussian-splatting not found, skipping reconstruction quality test"));
        return true;  // Skip, not fail
    }

    // Step 1: Capture and export dataset
    // ... (similar to full pipeline test)

    // Step 2: Run 3DGS training with 10% holdout
    // ... (execute Python training script)

    // Step 3: Render holdout views and compute metrics
    // ... (execute evaluation script)

    // Step 4: Parse results
    float ComputedPSNR = 0.0f;
    float ComputedSSIM = 0.0f;
    // ... (parse from evaluation output)

    TestTrue(FString::Printf(TEXT("PSNR >= %.1f dB"), TargetPSNR),
        ComputedPSNR >= TargetPSNR);
    TestTrue(FString::Printf(TEXT("SSIM >= %.2f"), TargetSSIM),
        ComputedSSIM >= TargetSSIM);

    AddInfo(FString::Printf(TEXT("Reconstruction PSNR: %.2f dB, SSIM: %.4f"),
        ComputedPSNR, ComputedSSIM));

    return true;
}
```

---

## 5. Test Data Requirements

### 5.1 Test Levels

| Level Name | Description | Complexity |
|------------|-------------|------------|
| TestLevel_Basic | Simple room with basic materials | Low |
| TestLevel_Complex | Multiple rooms, varied materials | Medium |
| TestLevel_Outdoor | Open environment with vegetation | High |
| TestLevel_Dynamic | Moving objects (optional) | High |

### 5.2 Test Asset Requirements

- Minimum 3 distinct material types
- At least one reflective surface
- Depth range from 1m to 100m
- Known reference points for validation
- Ground truth depth maps (for comparison)

---

## 6. Continuous Integration

### 6.1 CI Pipeline Stages

```yaml
# .github/workflows/ue5-3dgs-tests.yml
stages:
  - build
  - unit_tests
  - integration_tests
  - quality_tests

unit_tests:
  script:
    - UE5Editor.exe TestProject -ExecCmds="Automation RunTests UE5_3DGS.DEM;Quit"
  timeout: 10 minutes
  artifacts:
    reports:
      junit: TestResults/UnitTests.xml

integration_tests:
  script:
    - UE5Editor.exe TestProject -ExecCmds="Automation RunTests UE5_3DGS.Integration;Quit"
  timeout: 30 minutes
  only:
    - merge_requests
    - main

quality_tests:
  script:
    - UE5Editor.exe TestProject -ExecCmds="Automation RunTests UE5_3DGS.Quality;Quit"
  timeout: 2 hours
  only:
    - schedules
```

### 6.2 Coverage Requirements

| Module | Line Coverage | Branch Coverage |
|--------|--------------|-----------------|
| SCM | 85% | 75% |
| DEM | 90% | 80% |
| FCM | 90% | 85% |
| TRN | 80% | 70% |

---

*Document Version: 1.0*
*Last Updated: January 2025*
