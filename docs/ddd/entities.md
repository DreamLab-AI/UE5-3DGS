# Entities

## Overview

Entities are domain objects defined by their identity rather than their attributes. They have a lifecycle and maintain continuity through state changes.

---

## 1. CapturedFrame Entity

### Purpose
Represents a single captured frame with camera pose, render buffers, and metadata.

```cpp
// SCM/Entities/SCM_CapturedFrame.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"

namespace SCM
{
    /**
     * CapturedFrame Entity
     *
     * Identity: FrameIndex within parent CaptureSession
     * Lifecycle: Created during capture, immutable after creation
     */
    class FSCM_CapturedFrame
    {
    public:
        // Construction with identity
        FSCM_CapturedFrame(
            int32 InFrameIndex,
            double InTimestamp,
            const FTransform& InCameraTransform
        );

        // Identity (unique within session)
        int32 GetFrameIndex() const { return FrameIndex; }
        double GetTimestamp() const { return Timestamp; }

        // Camera transform (world space)
        const FTransform& GetCameraTransform() const { return CameraTransform; }
        FVector GetCameraLocation() const { return CameraTransform.GetLocation(); }
        FRotator GetCameraRotation() const { return CameraTransform.GetRotation().Rotator(); }
        FVector GetForwardVector() const { return CameraTransform.GetRotation().GetForwardVector(); }

        // Projection parameters
        void SetProjectionParameters(
            float InFocalLength_mm,
            float InSensorWidth_mm,
            float InSensorHeight_mm,
            const FVector2D& InPrincipalPoint
        );

        const FMatrix& GetProjectionMatrix() const { return ProjectionMatrix; }
        const FMatrix& GetViewMatrix() const { return ViewMatrix; }

        float GetFocalLength_mm() const { return FocalLength_mm; }
        float GetSensorWidth_mm() const { return SensorWidth_mm; }
        float GetSensorHeight_mm() const { return SensorHeight_mm; }
        const FVector2D& GetPrincipalPoint() const { return PrincipalPoint; }

        // Distortion coefficients
        void SetDistortionCoefficients(const TArray<float>& Coefficients);
        const TArray<float>& GetDistortionCoefficients() const;

        // Render buffers (set once during capture)
        void SetColorBuffer(TSharedPtr<class FDEM_ColorBuffer> Buffer);
        void SetDepthBuffer(TSharedPtr<class FDEM_DepthBuffer> Buffer);
        void SetNormalBuffer(TSharedPtr<class FDEM_NormalBuffer> Buffer);
        void SetMotionVectorBuffer(TSharedPtr<class FDEM_MotionVectorBuffer> Buffer);

        TSharedPtr<class FDEM_ColorBuffer> GetColorBuffer() const { return ColorBuffer; }
        TSharedPtr<class FDEM_DepthBuffer> GetDepthBuffer() const { return DepthBuffer; }
        TSharedPtr<class FDEM_NormalBuffer> GetNormalBuffer() const { return NormalBuffer; }
        TSharedPtr<class FDEM_MotionVectorBuffer> GetMotionVectorBuffer() const { return MotionVectorBuffer; }

        bool HasColorBuffer() const { return ColorBuffer.IsValid(); }
        bool HasDepthBuffer() const { return DepthBuffer.IsValid(); }
        bool HasNormalBuffer() const { return NormalBuffer.IsValid(); }

        // Scene metadata
        void SetVisibleActors(const TArray<FString>& Actors);
        void SetSceneBounds(const FBox& Bounds);
        void SetExposureValue(float EV);

        const TArray<FString>& GetVisibleActors() const { return VisibleActors; }
        const FBox& GetSceneBounds() const { return SceneBounds; }
        float GetExposureValue() const { return ExposureValue; }

        // Validation
        bool IsValid() const;
        bool HasRequiredBuffers() const;

        // Equality based on identity
        bool operator==(const FSCM_CapturedFrame& Other) const
        {
            return FrameIndex == Other.FrameIndex;
        }

    private:
        void ComputeMatrices();

        // Identity
        int32 FrameIndex;
        double Timestamp;

        // Camera pose
        FTransform CameraTransform;
        FMatrix ProjectionMatrix;
        FMatrix ViewMatrix;

        // Intrinsics
        float FocalLength_mm = 35.0f;
        float SensorWidth_mm = 36.0f;
        float SensorHeight_mm = 24.0f;
        FVector2D PrincipalPoint = FVector2D(0.5f, 0.5f);
        TArray<float> DistortionCoefficients;

        // Render buffers
        TSharedPtr<class FDEM_ColorBuffer> ColorBuffer;
        TSharedPtr<class FDEM_DepthBuffer> DepthBuffer;
        TSharedPtr<class FDEM_NormalBuffer> NormalBuffer;
        TSharedPtr<class FDEM_MotionVectorBuffer> MotionVectorBuffer;

        // Metadata
        TArray<FString> VisibleActors;
        FBox SceneBounds;
        float ExposureValue = 0.0f;
    };
}
```

### Identity Rules
- **FrameIndex**: Unique within parent CaptureSession
- **Immutability**: Core identity (index, timestamp, transform) immutable after creation

---

## 2. CameraParameters Entity

### Purpose
Represents camera calibration data in a format-agnostic manner, supporting conversion to multiple output conventions.

```cpp
// DEM/Entities/DEM_CameraParameters.h
#pragma once

#include "CoreMinimal.h"
#include "DEM_Types.h"

namespace DEM
{
    /**
     * Camera model enumeration (COLMAP compatible)
     */
    enum class ECameraModel : uint8
    {
        SIMPLE_PINHOLE = 0,  // f, cx, cy
        PINHOLE = 1,         // fx, fy, cx, cy
        SIMPLE_RADIAL = 2,   // f, cx, cy, k
        RADIAL = 3,          // f, cx, cy, k1, k2
        OPENCV = 4,          // fx, fy, cx, cy, k1, k2, p1, p2
        OPENCV_FISHEYE = 5,  // fx, fy, cx, cy, k1, k2, k3, k4
        FULL_OPENCV = 6,     // fx, fy, cx, cy, k1, k2, p1, p2, k3, k4, k5, k6
    };

    /**
     * CameraParameters Entity
     *
     * Identity: CameraId within parent SceneData
     * Supports multiple coordinate conventions for export
     */
    class FDEM_CameraParameters
    {
    public:
        FDEM_CameraParameters(int32 InCameraId);

        // Identity
        int32 GetCameraId() const { return CameraId; }

        // Image dimensions
        void SetImageSize(int32 Width, int32 Height);
        int32 GetImageWidth() const { return ImageWidth; }
        int32 GetImageHeight() const { return ImageHeight; }
        FIntPoint GetImageSize() const { return FIntPoint(ImageWidth, ImageHeight); }

        // Camera model
        void SetCameraModel(ECameraModel Model);
        ECameraModel GetCameraModel() const { return CameraModel; }
        FString GetCameraModelString() const;

        // Intrinsics
        void SetIntrinsics(
            float fx, float fy,
            float cx, float cy
        );

        void SetIntrinsicsFromFOV(
            float HorizontalFOV_Degrees,
            int32 Width,
            int32 Height
        );

        float GetFx() const { return Fx; }
        float GetFy() const { return Fy; }
        float GetCx() const { return Cx; }
        float GetCy() const { return Cy; }

        FMatrix GetIntrinsicMatrix() const;  // 3x3 K matrix

        // Distortion coefficients
        void SetDistortion(
            float k1, float k2, float k3,
            float p1, float p2
        );

        float GetK1() const { return K1; }
        float GetK2() const { return K2; }
        float GetK3() const { return K3; }
        float GetP1() const { return P1; }
        float GetP2() const { return P2; }

        TArray<float> GetDistortionCoefficients() const;
        bool HasDistortion() const;

        // Extrinsics (world-to-camera)
        void SetExtrinsics(const FTransform& WorldToCameraTransform);
        void SetExtrinsicsFromWorldPose(const FTransform& CameraWorldPose);

        const FTransform& GetWorldToCameraTransform() const { return WorldToCamera; }
        FTransform GetCameraWorldPose() const;

        FMatrix GetRotationMatrix() const;  // 3x3 R
        FVector GetTranslation() const;      // t
        FQuat GetRotationQuaternion() const; // (w, x, y, z)

        // Coordinate convention conversion
        void ConvertToConvention(EDEM_CoordinateConvention TargetConvention);
        EDEM_CoordinateConvention GetCurrentConvention() const { return CurrentConvention; }

        // COLMAP-specific output
        FString ToCOLMAPCameraLine() const;
        FString ToCOLMAPImageLine(const FString& ImageName) const;

        // Projection
        FVector2D ProjectPoint(const FVector& WorldPoint) const;
        FVector UnprojectPoint(const FVector2D& ImagePoint, float Depth) const;

        // Validation
        bool IsValid() const;

        // Equality
        bool operator==(const FDEM_CameraParameters& Other) const
        {
            return CameraId == Other.CameraId;
        }

    private:
        void UpdateMatrices();

        // Identity
        int32 CameraId;

        // Image size
        int32 ImageWidth = 1920;
        int32 ImageHeight = 1080;

        // Model
        ECameraModel CameraModel = ECameraModel::PINHOLE;

        // Intrinsics (pixels)
        float Fx = 1000.0f;
        float Fy = 1000.0f;
        float Cx = 960.0f;
        float Cy = 540.0f;

        // Distortion
        float K1 = 0.0f;
        float K2 = 0.0f;
        float K3 = 0.0f;
        float P1 = 0.0f;
        float P2 = 0.0f;

        // Extrinsics
        FTransform WorldToCamera;
        EDEM_CoordinateConvention CurrentConvention = EDEM_CoordinateConvention::UE5_LeftHanded_ZUp;

        // Cached matrices
        FMatrix IntrinsicMatrix;
        FMatrix RotationMatrix;
        mutable bool bMatricesDirty = true;
    };
}
```

### Identity Rules
- **CameraId**: Unique within parent SceneData
- **Convention Awareness**: Tracks current coordinate convention

---

## 3. GaussianSplat Entity

### Purpose
Represents a single 3D Gaussian primitive with position, appearance, and shape parameters.

```cpp
// TRN/Entities/TRN_GaussianSplat.h
#pragma once

#include "CoreMinimal.h"
#include "TRN_Types.h"

namespace TRN
{
    /**
     * GaussianSplat Entity
     *
     * Identity: SplatId within parent GaussianCloud
     * Mutable during training, immutable after export
     */
    class FTRN_GaussianSplat
    {
    public:
        FTRN_GaussianSplat(int32 InSplatId);

        // Identity
        int32 GetSplatId() const { return SplatId; }

        // Position (world space)
        void SetPosition(const FVector& InPosition);
        const FVector& GetPosition() const { return Position; }

        // Normal (optional, often unused in rendering)
        void SetNormal(const FVector& InNormal);
        const FVector& GetNormal() const { return Normal; }

        // Spherical Harmonics coefficients
        void SetSHDC(const FVector& DC);
        void SetSHRest(const TArray<float>& Rest);

        const FVector& GetSHDC() const { return SH_DC; }
        const TArray<float>& GetSHRest() const { return SH_Rest; }

        // Convenience: Get RGB colour (from DC component)
        FLinearColor GetBaseColor() const;
        void SetBaseColor(const FLinearColor& Color);

        // Opacity (pre-sigmoid)
        void SetOpacity(float InOpacity);
        float GetOpacity() const { return Opacity; }
        float GetActivatedOpacity() const;  // After sigmoid

        // Scale (log-space)
        void SetScale(const FVector& InScale);
        const FVector& GetScale() const { return Scale; }
        FVector GetActivatedScale() const;  // After exp

        // Rotation (quaternion)
        void SetRotation(const FQuat& InRotation);
        const FQuat& GetRotation() const { return Rotation; }
        FMatrix GetCovarianceMatrix() const;

        // Training state
        void SetGradient(const FVector& PositionGrad);
        const FVector& GetPositionGradient() const { return PositionGradient; }
        float GetGradientMagnitude() const;

        // Flags
        void MarkForDensification() { bMarkedForDensification = true; }
        void MarkForPruning() { bMarkedForPruning = true; }
        bool IsMarkedForDensification() const { return bMarkedForDensification; }
        bool IsMarkedForPruning() const { return bMarkedForPruning; }
        void ClearMarks();

        // Serialization
        TArray<uint8> ToBytes() const;
        static TSharedPtr<FTRN_GaussianSplat> FromBytes(const TArray<uint8>& Data);

        // PLY format
        void WriteToPLY(FArchive& Ar) const;
        static TSharedPtr<FTRN_GaussianSplat> ReadFromPLY(FArchive& Ar);

        // Validation
        bool IsValid() const;
        bool ShouldPrune(float OpacityThreshold, float ScaleThreshold) const;
        bool ShouldDensify(float GradThreshold) const;

        // Equality
        bool operator==(const FTRN_GaussianSplat& Other) const
        {
            return SplatId == Other.SplatId;
        }

    private:
        // Identity
        int32 SplatId;

        // Position (world space)
        FVector Position = FVector::ZeroVector;

        // Normal (often unused)
        FVector Normal = FVector::UpVector;

        // Spherical Harmonics (view-dependent colour)
        FVector SH_DC = FVector::ZeroVector;  // RGB DC component
        TArray<float> SH_Rest;  // Higher order SH (45 floats for degree 3)

        // Opacity (pre-activation)
        float Opacity = 0.0f;

        // Scale (log-space)
        FVector Scale = FVector::ZeroVector;

        // Rotation (unit quaternion)
        FQuat Rotation = FQuat::Identity;

        // Training state
        FVector PositionGradient = FVector::ZeroVector;
        bool bMarkedForDensification = false;
        bool bMarkedForPruning = false;
    };

    // SH coefficient count for different degrees
    constexpr int32 SH_DEGREE_0_COEFFS = 1;
    constexpr int32 SH_DEGREE_1_COEFFS = 4;
    constexpr int32 SH_DEGREE_2_COEFFS = 9;
    constexpr int32 SH_DEGREE_3_COEFFS = 16;

    // Standard 3DGS uses degree 3 = 16 coeffs per channel
    // Total = 3 (DC) + 15*3 (rest) = 48 float values for colour
    constexpr int32 SH_REST_SIZE = 45;  // (16-1) * 3 channels
}
```

### Identity Rules
- **SplatId**: Unique within parent GaussianCloud
- **Training Mutability**: Parameters modified during training iterations

---

## Entity Lifecycle Management

### CapturedFrame Lifecycle

```cpp
// Factory for creating frames during capture
class FSCM_FrameFactory
{
public:
    static TSharedPtr<FSCM_CapturedFrame> CreateFrame(
        int32 FrameIndex,
        USceneCaptureComponent2D* CaptureComponent,
        double Timestamp
    )
    {
        FTransform CameraTransform = CaptureComponent->GetComponentTransform();

        auto Frame = MakeShared<FSCM_CapturedFrame>(
            FrameIndex,
            Timestamp,
            CameraTransform
        );

        // Set projection parameters from capture component
        if (UCameraComponent* Camera = Cast<UCameraComponent>(CaptureComponent))
        {
            Frame->SetProjectionParameters(
                Camera->FieldOfView,
                Camera->AspectRatio,
                // ... sensor parameters
            );
        }

        return Frame;
    }
};
```

### CameraParameters Lifecycle

```cpp
// Conversion from UE5 camera to domain entity
class FDEM_CameraFactory
{
public:
    static TSharedPtr<FDEM_CameraParameters> FromCineCameraActor(
        int32 CameraId,
        ACineCameraActor* CineCamera,
        FIntPoint ImageSize
    )
    {
        auto Params = MakeShared<FDEM_CameraParameters>(CameraId);

        // Extract intrinsics from cine camera
        UCineCameraComponent* CineComp = CineCamera->GetCineCameraComponent();

        float FocalLength_mm = CineComp->CurrentFocalLength;
        float SensorWidth_mm = CineComp->Filmback.SensorWidth;
        float SensorHeight_mm = CineComp->Filmback.SensorHeight;

        // Convert focal length to pixels
        float Fx = (FocalLength_mm / SensorWidth_mm) * ImageSize.X;
        float Fy = (FocalLength_mm / SensorHeight_mm) * ImageSize.Y;

        Params->SetImageSize(ImageSize.X, ImageSize.Y);
        Params->SetIntrinsics(Fx, Fy, ImageSize.X / 2.0f, ImageSize.Y / 2.0f);
        Params->SetExtrinsicsFromWorldPose(CineCamera->GetActorTransform());

        return Params;
    }
};
```

### GaussianSplat Lifecycle

```cpp
// Splat creation during training initialization
class FTRN_SplatFactory
{
public:
    // Create from point cloud initialization
    static TSharedPtr<FTRN_GaussianSplat> CreateFromPoint(
        int32 SplatId,
        const FVector& Position,
        const FLinearColor& Color
    )
    {
        auto Splat = MakeShared<FTRN_GaussianSplat>(SplatId);

        Splat->SetPosition(Position);
        Splat->SetBaseColor(Color);
        Splat->SetOpacity(InverseActivation(0.1f));  // Initial opacity
        Splat->SetScale(FVector(-7.0f));  // exp(-7) ~ small initial scale
        Splat->SetRotation(FQuat::Identity);

        // Initialize SH rest to zero
        TArray<float> SHRest;
        SHRest.SetNumZeroed(TRN::SH_REST_SIZE);
        Splat->SetSHRest(SHRest);

        return Splat;
    }

    // Clone for densification
    static TSharedPtr<FTRN_GaussianSplat> Clone(
        const FTRN_GaussianSplat& Source,
        int32 NewSplatId
    )
    {
        auto Splat = MakeShared<FTRN_GaussianSplat>(NewSplatId);

        Splat->SetPosition(Source.GetPosition());
        Splat->SetNormal(Source.GetNormal());
        Splat->SetSHDC(Source.GetSHDC());
        Splat->SetSHRest(Source.GetSHRest());
        Splat->SetOpacity(Source.GetOpacity());
        Splat->SetScale(Source.GetScale());
        Splat->SetRotation(Source.GetRotation());

        return Splat;
    }
};
```
