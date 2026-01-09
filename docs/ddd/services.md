# Domain Services

## Overview

Domain Services encapsulate domain logic that does not naturally fit within an entity or value object. They are stateless operations that work with multiple domain objects.

---

## 1. TrajectoryGenerator Service

### Purpose
Generates optimal camera trajectories for scene coverage.

```cpp
// SCM/Services/SCM_TrajectoryGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "SCM_Types.h"

namespace SCM
{
    /**
     * Trajectory parameters structure
     */
    struct FTrajectoryParams
    {
        // Common parameters
        FBox SceneBounds;
        FVector FocusPoint;
        int32 FrameCount = 100;

        // Spherical parameters
        float Radius = 500.0f;
        float MinElevation = -30.0f;
        float MaxElevation = 60.0f;

        // Orbital parameters
        int32 Orbits = 3;
        float OrbitRadiusStep = 100.0f;

        // Spline parameters
        TArray<FTransform> Keyframes;
        float SplineSmoothing = 0.5f;

        // Coverage parameters
        float TargetCoverage = 0.95f;
        float MinBaseline = 50.0f;  // Minimum distance between cameras
    };

    /**
     * Trajectory result structure
     */
    struct FTrajectory
    {
        TArray<FTransform> CameraPoses;
        TArray<FVector> LookAtTargets;
        float EstimatedCoverage;
        float TotalPathLength;
    };

    /**
     * TrajectoryGenerator Domain Service
     *
     * Generates camera trajectories for optimal scene coverage.
     */
    class FSCM_TrajectoryGenerator
    {
    public:
        /**
         * Generate spherical trajectory using Fibonacci lattice
         *
         * @param Params Trajectory parameters including radius and elevation limits
         * @return Generated trajectory with camera poses
         */
        FTrajectory GenerateSpherical(const FTrajectoryParams& Params) const
        {
            FTrajectory Result;
            Result.CameraPoses.Reserve(Params.FrameCount);

            // Fibonacci lattice for uniform sphere sampling
            const float GoldenRatio = (1.0f + FMath::Sqrt(5.0f)) / 2.0f;
            const float AngleIncrement = PI * 2.0f * GoldenRatio;

            for (int32 i = 0; i < Params.FrameCount; ++i)
            {
                // Compute spherical coordinates
                float T = static_cast<float>(i) / (Params.FrameCount - 1);
                float Inclination = FMath::Acos(1.0f - 2.0f * T);
                float Azimuth = AngleIncrement * i;

                // Apply elevation constraints
                float Elevation = FMath::RadiansToDegrees(PI / 2.0f - Inclination);
                Elevation = FMath::Clamp(Elevation, Params.MinElevation, Params.MaxElevation);
                Inclination = PI / 2.0f - FMath::DegreesToRadians(Elevation);

                // Compute position on sphere
                FVector Position;
                Position.X = Params.Radius * FMath::Sin(Inclination) * FMath::Cos(Azimuth);
                Position.Y = Params.Radius * FMath::Sin(Inclination) * FMath::Sin(Azimuth);
                Position.Z = Params.Radius * FMath::Cos(Inclination);
                Position += Params.FocusPoint;

                // Compute rotation to look at focus point
                FRotator LookAtRotation = (Params.FocusPoint - Position).Rotation();

                Result.CameraPoses.Add(FTransform(LookAtRotation, Position));
                Result.LookAtTargets.Add(Params.FocusPoint);
            }

            Result.EstimatedCoverage = EstimateCoverage(Result, Params.SceneBounds);
            Result.TotalPathLength = ComputePathLength(Result);

            return Result;
        }

        /**
         * Generate orbital trajectory with multiple rings
         */
        FTrajectory GenerateOrbital(const FTrajectoryParams& Params) const
        {
            FTrajectory Result;

            int32 FramesPerOrbit = Params.FrameCount / Params.Orbits;

            for (int32 Orbit = 0; Orbit < Params.Orbits; ++Orbit)
            {
                float CurrentRadius = Params.Radius + Orbit * Params.OrbitRadiusStep;
                float Elevation = FMath::Lerp(
                    Params.MinElevation,
                    Params.MaxElevation,
                    static_cast<float>(Orbit) / (Params.Orbits - 1)
                );

                for (int32 i = 0; i < FramesPerOrbit; ++i)
                {
                    float Angle = (2.0f * PI * i) / FramesPerOrbit;

                    FVector Position;
                    Position.X = CurrentRadius * FMath::Cos(Angle);
                    Position.Y = CurrentRadius * FMath::Sin(Angle);
                    Position.Z = CurrentRadius * FMath::Tan(FMath::DegreesToRadians(Elevation));
                    Position += Params.FocusPoint;

                    FRotator LookAt = (Params.FocusPoint - Position).Rotation();
                    Result.CameraPoses.Add(FTransform(LookAt, Position));
                    Result.LookAtTargets.Add(Params.FocusPoint);
                }
            }

            Result.EstimatedCoverage = EstimateCoverage(Result, Params.SceneBounds);
            Result.TotalPathLength = ComputePathLength(Result);

            return Result;
        }

        /**
         * Generate flythrough trajectory from spline keyframes
         */
        FTrajectory GenerateFlythrough(const FTrajectoryParams& Params) const
        {
            FTrajectory Result;

            if (Params.Keyframes.Num() < 2)
            {
                return Result;
            }

            // Create Catmull-Rom spline through keyframes
            for (int32 i = 0; i < Params.FrameCount; ++i)
            {
                float T = static_cast<float>(i) / (Params.FrameCount - 1);
                FTransform Interpolated = InterpolateCatmullRom(Params.Keyframes, T);
                Result.CameraPoses.Add(Interpolated);

                // Look direction from transform
                Result.LookAtTargets.Add(
                    Interpolated.GetLocation() +
                    Interpolated.GetRotation().GetForwardVector() * 1000.0f
                );
            }

            Result.EstimatedCoverage = EstimateCoverage(Result, Params.SceneBounds);
            Result.TotalPathLength = ComputePathLength(Result);

            return Result;
        }

        /**
         * Generate adaptive coverage trajectory (iterative refinement)
         */
        FTrajectory GenerateAdaptive(
            const FTrajectoryParams& Params,
            TFunction<float(const FTransform&)> CoverageEvaluator
        ) const
        {
            // Start with spherical baseline
            FTrajectory Result = GenerateSpherical(Params);

            // Iterative refinement
            const int32 MaxIterations = 10;
            for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
            {
                if (Result.EstimatedCoverage >= Params.TargetCoverage)
                {
                    break;
                }

                // Find areas with poor coverage
                TArray<FVector> PoorCoverageRegions = IdentifyPoorCoverage(
                    Result,
                    Params.SceneBounds,
                    CoverageEvaluator
                );

                // Add cameras targeting poor regions
                for (const FVector& Target : PoorCoverageRegions)
                {
                    FTransform NewPose = ComputeBestViewForTarget(
                        Target,
                        Result.CameraPoses,
                        Params.MinBaseline
                    );
                    Result.CameraPoses.Add(NewPose);
                    Result.LookAtTargets.Add(Target);
                }

                Result.EstimatedCoverage = EstimateCoverage(Result, Params.SceneBounds);
            }

            return Result;
        }

        /**
         * Generate random stratified trajectory
         */
        FTrajectory GenerateRandomStratified(const FTrajectoryParams& Params) const
        {
            FTrajectory Result;

            // Divide space into strata
            int32 StrataPerAxis = FMath::CeilToInt(FMath::Pow(
                static_cast<float>(Params.FrameCount), 1.0f / 3.0f
            ));

            FVector BoundsSize = Params.SceneBounds.GetSize();
            FVector StrataSize = BoundsSize / StrataPerAxis;

            for (int32 i = 0; i < Params.FrameCount; ++i)
            {
                // Select random stratum
                int32 StrataX = FMath::RandRange(0, StrataPerAxis - 1);
                int32 StrataY = FMath::RandRange(0, StrataPerAxis - 1);
                int32 StrataZ = FMath::RandRange(0, StrataPerAxis - 1);

                // Random position within stratum
                FVector Position = Params.SceneBounds.Min +
                    FVector(StrataX, StrataY, StrataZ) * StrataSize +
                    FVector(
                        FMath::FRand() * StrataSize.X,
                        FMath::FRand() * StrataSize.Y,
                        FMath::FRand() * StrataSize.Z
                    );

                // Random look direction biased toward scene center
                FVector ToCenter = Params.FocusPoint - Position;
                FVector RandomDir = FMath::VRand();
                FVector LookDir = FMath::Lerp(ToCenter.GetSafeNormal(), RandomDir, 0.3f);

                Result.CameraPoses.Add(FTransform(LookDir.Rotation(), Position));
                Result.LookAtTargets.Add(Position + LookDir * 1000.0f);
            }

            Result.EstimatedCoverage = EstimateCoverage(Result, Params.SceneBounds);
            Result.TotalPathLength = ComputePathLength(Result);

            return Result;
        }

    private:
        float EstimateCoverage(
            const FTrajectory& Trajectory,
            const FBox& Bounds
        ) const;

        float ComputePathLength(const FTrajectory& Trajectory) const;

        FTransform InterpolateCatmullRom(
            const TArray<FTransform>& Keyframes,
            float T
        ) const;

        TArray<FVector> IdentifyPoorCoverage(
            const FTrajectory& CurrentTrajectory,
            const FBox& Bounds,
            TFunction<float(const FTransform&)> Evaluator
        ) const;

        FTransform ComputeBestViewForTarget(
            const FVector& Target,
            const TArray<FTransform>& ExistingPoses,
            float MinBaseline
        ) const;
    };
}
```

---

## 2. BufferExtractor Service

### Purpose
Extracts and converts render buffers from UE5 render targets.

```cpp
// DEM/Services/DEM_BufferExtractor.h
#pragma once

#include "CoreMinimal.h"
#include "DEM_Types.h"
#include "Engine/TextureRenderTarget2D.h"

namespace DEM
{
    /**
     * Buffer extraction options
     */
    struct FBufferExtractionOptions
    {
        EDEM_ColorSpace TargetColorSpace = EDEM_ColorSpace::sRGB;
        EDEM_DepthFormat DepthFormat = EDEM_DepthFormat::LinearMetres;
        EDEM_NormalSpace NormalSpace = EDEM_NormalSpace::WorldSpace;
        bool bFlipVertical = false;
        bool bPremultiplyAlpha = false;
    };

    /**
     * BufferExtractor Domain Service
     *
     * Extracts render buffers and converts them to domain value objects.
     */
    class FDEM_BufferExtractor
    {
    public:
        /**
         * Extract colour buffer from render target
         */
        TSharedPtr<FColorBuffer> ExtractColorBuffer(
            UTextureRenderTarget2D* RenderTarget,
            const FBufferExtractionOptions& Options = {}
        ) const
        {
            if (!RenderTarget)
            {
                return nullptr;
            }

            FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
            if (!Resource)
            {
                return nullptr;
            }

            int32 Width = RenderTarget->SizeX;
            int32 Height = RenderTarget->SizeY;

            TArray<FColor> Pixels;
            Pixels.SetNumUninitialized(Width * Height);

            FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
            Resource->ReadPixels(Pixels, ReadFlags);

            // Convert to byte array
            TArray<uint8> Data;
            Data.SetNumUninitialized(Width * Height * 4);

            for (int32 i = 0; i < Pixels.Num(); ++i)
            {
                int32 SourceIndex = i;
                if (Options.bFlipVertical)
                {
                    int32 Y = i / Width;
                    int32 X = i % Width;
                    SourceIndex = (Height - 1 - Y) * Width + X;
                }

                const FColor& Pixel = Pixels[SourceIndex];
                int32 DestIndex = i * 4;
                Data[DestIndex + 0] = Pixel.R;
                Data[DestIndex + 1] = Pixel.G;
                Data[DestIndex + 2] = Pixel.B;
                Data[DestIndex + 3] = Pixel.A;
            }

            EBufferFormat Format = Options.TargetColorSpace == EDEM_ColorSpace::sRGB
                ? EBufferFormat::R8G8B8A8_SRGB
                : EBufferFormat::R8G8B8A8_Linear;

            return MakeShared<FColorBuffer>(
                FColorBuffer::Create(Width, Height, Format, MoveTemp(Data))
            );
        }

        /**
         * Extract depth buffer with reverse-Z conversion
         */
        TSharedPtr<FDepthBuffer> ExtractDepthBuffer(
            UTextureRenderTarget2D* RenderTarget,
            float NearPlane,
            float FarPlane,
            const FBufferExtractionOptions& Options = {}
        ) const
        {
            if (!RenderTarget)
            {
                return nullptr;
            }

            FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
            if (!Resource)
            {
                return nullptr;
            }

            int32 Width = RenderTarget->SizeX;
            int32 Height = RenderTarget->SizeY;

            TArray<FLinearColor> FloatPixels;
            FloatPixels.SetNumUninitialized(Width * Height);

            FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
            Resource->ReadLinearColorPixels(FloatPixels, ReadFlags);

            // Convert reverse-Z to linear depth
            TArray<float> DepthData;
            DepthData.SetNumUninitialized(Width * Height);

            for (int32 i = 0; i < FloatPixels.Num(); ++i)
            {
                int32 SourceIndex = i;
                if (Options.bFlipVertical)
                {
                    int32 Y = i / Width;
                    int32 X = i % Width;
                    SourceIndex = (Height - 1 - Y) * Width + X;
                }

                float ReverseZ = FloatPixels[SourceIndex].R;

                // Convert UE5 reverse-Z to linear depth
                float LinearDepth = ConvertReverseZToLinear(
                    ReverseZ, NearPlane, FarPlane
                );

                // Convert to target format
                switch (Options.DepthFormat)
                {
                    case EDEM_DepthFormat::LinearMetres:
                        DepthData[i] = LinearDepth * 0.01f;  // cm to metres
                        break;
                    case EDEM_DepthFormat::LinearCentimetres:
                        DepthData[i] = LinearDepth;
                        break;
                    case EDEM_DepthFormat::Normalised:
                        DepthData[i] = (LinearDepth - NearPlane) / (FarPlane - NearPlane);
                        break;
                    case EDEM_DepthFormat::InverseDepth:
                        DepthData[i] = 1.0f / LinearDepth;
                        break;
                }
            }

            float FarInUnits = Options.DepthFormat == EDEM_DepthFormat::LinearMetres
                ? FarPlane * 0.01f
                : FarPlane;
            float NearInUnits = Options.DepthFormat == EDEM_DepthFormat::LinearMetres
                ? NearPlane * 0.01f
                : NearPlane;

            return MakeShared<FDepthBuffer>(
                FDepthBuffer::Create(Width, Height, MoveTemp(DepthData), NearInUnits, FarInUnits)
            );
        }

        /**
         * Extract world-space normal buffer
         */
        TSharedPtr<FNormalBuffer> ExtractNormalBuffer(
            UTextureRenderTarget2D* RenderTarget,
            const FBufferExtractionOptions& Options = {}
        ) const
        {
            if (!RenderTarget)
            {
                return nullptr;
            }

            FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
            if (!Resource)
            {
                return nullptr;
            }

            int32 Width = RenderTarget->SizeX;
            int32 Height = RenderTarget->SizeY;

            TArray<FLinearColor> Pixels;
            Pixels.SetNumUninitialized(Width * Height);

            FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
            Resource->ReadLinearColorPixels(Pixels, ReadFlags);

            TArray<FVector> NormalData;
            NormalData.SetNumUninitialized(Width * Height);

            for (int32 i = 0; i < Pixels.Num(); ++i)
            {
                int32 SourceIndex = i;
                if (Options.bFlipVertical)
                {
                    int32 Y = i / Width;
                    int32 X = i % Width;
                    SourceIndex = (Height - 1 - Y) * Width + X;
                }

                const FLinearColor& Pixel = Pixels[SourceIndex];

                // Convert from [0,1] to [-1,1]
                FVector Normal(
                    Pixel.R * 2.0f - 1.0f,
                    Pixel.G * 2.0f - 1.0f,
                    Pixel.B * 2.0f - 1.0f
                );

                Normal.Normalize();
                NormalData[i] = Normal;
            }

            return MakeShared<FNormalBuffer>(Width, Height, MoveTemp(NormalData));
        }

    private:
        float ConvertReverseZToLinear(
            float ReverseZ,
            float NearPlane,
            float FarPlane
        ) const
        {
            // UE5 uses reverse-Z infinite far plane projection
            // Z_buffer = Near / Z_linear (for infinite far plane)
            // For finite far plane: Z_buffer = (Far * Near) / (Far - Z * (Far - Near))

            if (ReverseZ <= 0.0f)
            {
                return FarPlane;
            }

            return (FarPlane * NearPlane) /
                (FarPlane - ReverseZ * (FarPlane - NearPlane));
        }
    };
}
```

---

## 3. COLMAPWriter Service

### Purpose
Writes camera parameters and metadata in COLMAP format.

```cpp
// FCM/Services/FCM_COLMAPWriter.h
#pragma once

#include "CoreMinimal.h"
#include "FCM_Types.h"
#include "DEM/DEM_CameraParameters.h"

namespace FCM
{
    /**
     * COLMAP write options
     */
    struct FCOLMAPWriteOptions
    {
        bool bBinaryFormat = true;
        bool bWritePoints3D = false;
        FString ImageExtension = TEXT("png");
    };

    /**
     * COLMAPWriter Domain Service
     *
     * Exports camera parameters to COLMAP text and binary formats.
     */
    class FFCM_COLMAPWriter
    {
    public:
        /**
         * Write complete COLMAP dataset
         */
        bool WriteDataset(
            const FString& OutputPath,
            const TArray<TSharedPtr<DEM::FDEM_CameraParameters>>& Cameras,
            const TArray<FString>& ImagePaths,
            const FCOLMAPWriteOptions& Options = {}
        )
        {
            // Create directory structure
            FString SparsePath = OutputPath / TEXT("sparse") / TEXT("0");
            IFileManager::Get().MakeDirectory(*SparsePath, true);

            bool bSuccess = true;

            if (Options.bBinaryFormat)
            {
                bSuccess &= WriteCamerasBinary(SparsePath / TEXT("cameras.bin"), Cameras);
                bSuccess &= WriteImagesBinary(SparsePath / TEXT("images.bin"), Cameras, ImagePaths);
                bSuccess &= WritePoints3DBinary(SparsePath / TEXT("points3D.bin"), {});
            }
            else
            {
                bSuccess &= WriteCamerasText(SparsePath / TEXT("cameras.txt"), Cameras);
                bSuccess &= WriteImagesText(SparsePath / TEXT("images.txt"), Cameras, ImagePaths);
                bSuccess &= WritePoints3DText(SparsePath / TEXT("points3D.txt"), {});
            }

            return bSuccess;
        }

        /**
         * Write cameras.txt
         *
         * Format: CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]
         */
        bool WriteCamerasText(
            const FString& FilePath,
            const TArray<TSharedPtr<DEM::FDEM_CameraParameters>>& Cameras
        )
        {
            FString Content;

            // Header
            Content += TEXT("# Camera list with one line of data per camera:\n");
            Content += TEXT("#   CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]\n");
            Content += FString::Printf(TEXT("# Number of cameras: %d\n"), Cameras.Num());

            for (const auto& Camera : Cameras)
            {
                Content += Camera->ToCOLMAPCameraLine() + TEXT("\n");
            }

            return FFileHelper::SaveStringToFile(Content, *FilePath);
        }

        /**
         * Write cameras.bin (binary format)
         */
        bool WriteCamerasBinary(
            const FString& FilePath,
            const TArray<TSharedPtr<DEM::FDEM_CameraParameters>>& Cameras
        )
        {
            TArray<uint8> Data;
            FMemoryWriter Writer(Data);

            // Number of cameras (uint64)
            uint64 NumCameras = Cameras.Num();
            Writer << NumCameras;

            for (const auto& Camera : Cameras)
            {
                // camera_id (uint32)
                uint32 CameraId = Camera->GetCameraId();
                Writer.Serialize(&CameraId, sizeof(uint32));

                // model_id (int32)
                int32 ModelId = static_cast<int32>(Camera->GetCameraModel());
                Writer.Serialize(&ModelId, sizeof(int32));

                // width (uint64)
                uint64 Width = Camera->GetImageWidth();
                Writer << Width;

                // height (uint64)
                uint64 Height = Camera->GetImageHeight();
                Writer << Height;

                // params (depends on model)
                WriteModelParams(Writer, Camera);
            }

            return FFileHelper::SaveArrayToFile(Data, *FilePath);
        }

        /**
         * Write images.txt
         *
         * Format: IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME
         */
        bool WriteImagesText(
            const FString& FilePath,
            const TArray<TSharedPtr<DEM::FDEM_CameraParameters>>& Cameras,
            const TArray<FString>& ImagePaths
        )
        {
            FString Content;

            // Header
            Content += TEXT("# Image list with two lines of data per image:\n");
            Content += TEXT("#   IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME\n");
            Content += TEXT("#   POINTS2D[] as (X, Y, POINT3D_ID)\n");
            Content += FString::Printf(TEXT("# Number of images: %d\n"), Cameras.Num());

            for (int32 i = 0; i < Cameras.Num(); ++i)
            {
                FString ImageName = FPaths::GetCleanFilename(ImagePaths[i]);
                Content += Cameras[i]->ToCOLMAPImageLine(ImageName) + TEXT("\n");
                Content += TEXT("\n");  // Empty line for POINTS2D
            }

            return FFileHelper::SaveStringToFile(Content, *FilePath);
        }

        /**
         * Write images.bin (binary format)
         */
        bool WriteImagesBinary(
            const FString& FilePath,
            const TArray<TSharedPtr<DEM::FDEM_CameraParameters>>& Cameras,
            const TArray<FString>& ImagePaths
        )
        {
            TArray<uint8> Data;
            FMemoryWriter Writer(Data);

            // Number of images (uint64)
            uint64 NumImages = Cameras.Num();
            Writer << NumImages;

            for (int32 i = 0; i < Cameras.Num(); ++i)
            {
                const auto& Camera = Cameras[i];

                // image_id (uint32)
                uint32 ImageId = i + 1;
                Writer.Serialize(&ImageId, sizeof(uint32));

                // qw, qx, qy, qz (double[4])
                FQuat Q = Camera->GetRotationQuaternion();
                double qw = Q.W, qx = Q.X, qy = Q.Y, qz = Q.Z;
                Writer << qw << qx << qy << qz;

                // tx, ty, tz (double[3])
                FVector T = Camera->GetTranslation();
                double tx = T.X, ty = T.Y, tz = T.Z;
                Writer << tx << ty << tz;

                // camera_id (uint32)
                uint32 CameraId = Camera->GetCameraId();
                Writer.Serialize(&CameraId, sizeof(uint32));

                // name (null-terminated string)
                FString ImageName = FPaths::GetCleanFilename(ImagePaths[i]);
                for (TCHAR C : ImageName)
                {
                    char Ch = static_cast<char>(C);
                    Writer.Serialize(&Ch, 1);
                }
                char NullTerm = '\0';
                Writer.Serialize(&NullTerm, 1);

                // num_points2D (uint64) - empty for synthetic data
                uint64 NumPoints2D = 0;
                Writer << NumPoints2D;
            }

            return FFileHelper::SaveArrayToFile(Data, *FilePath);
        }

        /**
         * Write points3D.txt (can be empty for 3DGS)
         */
        bool WritePoints3DText(
            const FString& FilePath,
            const TArray<FVector>& Points
        )
        {
            FString Content;
            Content += TEXT("# 3D point list\n");
            Content += FString::Printf(TEXT("# Number of points: %d\n"), Points.Num());

            for (int32 i = 0; i < Points.Num(); ++i)
            {
                Content += FString::Printf(
                    TEXT("%d %f %f %f 255 255 255 0\n"),
                    i + 1,
                    Points[i].X,
                    Points[i].Y,
                    Points[i].Z
                );
            }

            return FFileHelper::SaveStringToFile(Content, *FilePath);
        }

        /**
         * Write points3D.bin (binary format)
         */
        bool WritePoints3DBinary(
            const FString& FilePath,
            const TArray<FVector>& Points
        )
        {
            TArray<uint8> Data;
            FMemoryWriter Writer(Data);

            uint64 NumPoints = Points.Num();
            Writer << NumPoints;

            // Empty for synthetic data - 3DGS will generate points

            return FFileHelper::SaveArrayToFile(Data, *FilePath);
        }

    private:
        void WriteModelParams(
            FMemoryWriter& Writer,
            const TSharedPtr<DEM::FDEM_CameraParameters>& Camera
        )
        {
            switch (Camera->GetCameraModel())
            {
                case DEM::ECameraModel::PINHOLE:
                {
                    double fx = Camera->GetFx();
                    double fy = Camera->GetFy();
                    double cx = Camera->GetCx();
                    double cy = Camera->GetCy();
                    Writer << fx << fy << cx << cy;
                    break;
                }
                case DEM::ECameraModel::OPENCV:
                {
                    double fx = Camera->GetFx();
                    double fy = Camera->GetFy();
                    double cx = Camera->GetCx();
                    double cy = Camera->GetCy();
                    double k1 = Camera->GetK1();
                    double k2 = Camera->GetK2();
                    double p1 = Camera->GetP1();
                    double p2 = Camera->GetP2();
                    Writer << fx << fy << cx << cy << k1 << k2 << p1 << p2;
                    break;
                }
                default:
                    break;
            }
        }
    };
}
```

---

## 4. PLYWriter Service

### Purpose
Writes 3DGS-compatible PLY files.

```cpp
// FCM/Services/FCM_PLYWriter.h
#pragma once

#include "CoreMinimal.h"
#include "TRN/TRN_GaussianSplat.h"

namespace FCM
{
    /**
     * PLY write options
     */
    struct FPLYWriteOptions
    {
        bool bBinaryFormat = true;
        bool bWriteNormals = true;
        bool bWriteFullSH = true;  // All 48 SH coefficients
        int32 SHDegree = 3;
    };

    /**
     * PLYWriter Domain Service
     *
     * Exports gaussian splats to PLY format.
     */
    class FFCM_PLYWriter
    {
    public:
        /**
         * Write gaussian cloud to PLY file
         */
        bool WriteGaussians(
            const FString& FilePath,
            const TArray<TSharedPtr<TRN::FTRN_GaussianSplat>>& Splats,
            const FPLYWriteOptions& Options = {}
        )
        {
            if (Options.bBinaryFormat)
            {
                return WriteBinaryPLY(FilePath, Splats, Options);
            }
            else
            {
                return WriteASCIIPLY(FilePath, Splats, Options);
            }
        }

    private:
        FString GenerateHeader(
            int32 VertexCount,
            const FPLYWriteOptions& Options
        ) const
        {
            FString Header;

            Header += TEXT("ply\n");
            Header += Options.bBinaryFormat
                ? TEXT("format binary_little_endian 1.0\n")
                : TEXT("format ascii 1.0\n");

            Header += FString::Printf(TEXT("element vertex %d\n"), VertexCount);

            // Position
            Header += TEXT("property float x\n");
            Header += TEXT("property float y\n");
            Header += TEXT("property float z\n");

            // Normal (optional)
            if (Options.bWriteNormals)
            {
                Header += TEXT("property float nx\n");
                Header += TEXT("property float ny\n");
                Header += TEXT("property float nz\n");
            }

            // Spherical Harmonics DC
            Header += TEXT("property float f_dc_0\n");
            Header += TEXT("property float f_dc_1\n");
            Header += TEXT("property float f_dc_2\n");

            // Spherical Harmonics rest
            if (Options.bWriteFullSH)
            {
                for (int32 i = 0; i < TRN::SH_REST_SIZE; ++i)
                {
                    Header += FString::Printf(TEXT("property float f_rest_%d\n"), i);
                }
            }

            // Opacity
            Header += TEXT("property float opacity\n");

            // Scale
            Header += TEXT("property float scale_0\n");
            Header += TEXT("property float scale_1\n");
            Header += TEXT("property float scale_2\n");

            // Rotation
            Header += TEXT("property float rot_0\n");
            Header += TEXT("property float rot_1\n");
            Header += TEXT("property float rot_2\n");
            Header += TEXT("property float rot_3\n");

            Header += TEXT("end_header\n");

            return Header;
        }

        bool WriteBinaryPLY(
            const FString& FilePath,
            const TArray<TSharedPtr<TRN::FTRN_GaussianSplat>>& Splats,
            const FPLYWriteOptions& Options
        )
        {
            FString Header = GenerateHeader(Splats.Num(), Options);

            TArray<uint8> Data;

            // Write header as ASCII
            FTCHARToUTF8 HeaderUTF8(*Header);
            Data.Append(
                reinterpret_cast<const uint8*>(HeaderUTF8.Get()),
                HeaderUTF8.Length()
            );

            // Write binary data
            for (const auto& Splat : Splats)
            {
                // Position
                FVector Pos = Splat->GetPosition();
                Data.Append(reinterpret_cast<uint8*>(&Pos.X), sizeof(float));
                Data.Append(reinterpret_cast<uint8*>(&Pos.Y), sizeof(float));
                Data.Append(reinterpret_cast<uint8*>(&Pos.Z), sizeof(float));

                // Normal
                if (Options.bWriteNormals)
                {
                    FVector Normal = Splat->GetNormal();
                    Data.Append(reinterpret_cast<uint8*>(&Normal.X), sizeof(float));
                    Data.Append(reinterpret_cast<uint8*>(&Normal.Y), sizeof(float));
                    Data.Append(reinterpret_cast<uint8*>(&Normal.Z), sizeof(float));
                }

                // SH DC
                FVector DC = Splat->GetSHDC();
                Data.Append(reinterpret_cast<uint8*>(&DC.X), sizeof(float));
                Data.Append(reinterpret_cast<uint8*>(&DC.Y), sizeof(float));
                Data.Append(reinterpret_cast<uint8*>(&DC.Z), sizeof(float));

                // SH Rest
                if (Options.bWriteFullSH)
                {
                    const TArray<float>& Rest = Splat->GetSHRest();
                    for (int32 i = 0; i < TRN::SH_REST_SIZE; ++i)
                    {
                        float Value = i < Rest.Num() ? Rest[i] : 0.0f;
                        Data.Append(reinterpret_cast<uint8*>(&Value), sizeof(float));
                    }
                }

                // Opacity
                float Opacity = Splat->GetOpacity();
                Data.Append(reinterpret_cast<uint8*>(&Opacity), sizeof(float));

                // Scale
                FVector Scale = Splat->GetScale();
                Data.Append(reinterpret_cast<uint8*>(&Scale.X), sizeof(float));
                Data.Append(reinterpret_cast<uint8*>(&Scale.Y), sizeof(float));
                Data.Append(reinterpret_cast<uint8*>(&Scale.Z), sizeof(float));

                // Rotation (w, x, y, z)
                FQuat Rot = Splat->GetRotation();
                Data.Append(reinterpret_cast<uint8*>(&Rot.W), sizeof(float));
                Data.Append(reinterpret_cast<uint8*>(&Rot.X), sizeof(float));
                Data.Append(reinterpret_cast<uint8*>(&Rot.Y), sizeof(float));
                Data.Append(reinterpret_cast<uint8*>(&Rot.Z), sizeof(float));
            }

            return FFileHelper::SaveArrayToFile(Data, *FilePath);
        }

        bool WriteASCIIPLY(
            const FString& FilePath,
            const TArray<TSharedPtr<TRN::FTRN_GaussianSplat>>& Splats,
            const FPLYWriteOptions& Options
        )
        {
            FString Content = GenerateHeader(Splats.Num(), Options);

            for (const auto& Splat : Splats)
            {
                FVector Pos = Splat->GetPosition();
                Content += FString::Printf(TEXT("%f %f %f "), Pos.X, Pos.Y, Pos.Z);

                if (Options.bWriteNormals)
                {
                    FVector Normal = Splat->GetNormal();
                    Content += FString::Printf(TEXT("%f %f %f "), Normal.X, Normal.Y, Normal.Z);
                }

                FVector DC = Splat->GetSHDC();
                Content += FString::Printf(TEXT("%f %f %f "), DC.X, DC.Y, DC.Z);

                if (Options.bWriteFullSH)
                {
                    const TArray<float>& Rest = Splat->GetSHRest();
                    for (int32 i = 0; i < TRN::SH_REST_SIZE; ++i)
                    {
                        float Value = i < Rest.Num() ? Rest[i] : 0.0f;
                        Content += FString::Printf(TEXT("%f "), Value);
                    }
                }

                float Opacity = Splat->GetOpacity();
                Content += FString::Printf(TEXT("%f "), Opacity);

                FVector Scale = Splat->GetScale();
                Content += FString::Printf(TEXT("%f %f %f "), Scale.X, Scale.Y, Scale.Z);

                FQuat Rot = Splat->GetRotation();
                Content += FString::Printf(TEXT("%f %f %f %f\n"), Rot.W, Rot.X, Rot.Y, Rot.Z);
            }

            return FFileHelper::SaveStringToFile(Content, *FilePath);
        }
    };
}
```

---

## Service Dependency Injection

```cpp
// Shared/Services/ServiceLocator.h
#pragma once

#include "CoreMinimal.h"

namespace Shared
{
    /**
     * Simple service locator for domain services
     */
    class FServiceLocator
    {
    public:
        static FServiceLocator& Get()
        {
            static FServiceLocator Instance;
            return Instance;
        }

        template<typename TService>
        void Register(TSharedPtr<TService> Service)
        {
            FString TypeName = TService::StaticStruct()->GetName();
            Services.Add(TypeName, StaticCastSharedPtr<void>(Service));
        }

        template<typename TService>
        TSharedPtr<TService> Resolve()
        {
            FString TypeName = TService::StaticStruct()->GetName();
            auto* Found = Services.Find(TypeName);
            if (Found)
            {
                return StaticCastSharedPtr<TService>(*Found);
            }
            return nullptr;
        }

    private:
        TMap<FString, TSharedPtr<void>> Services;
    };
}
```
