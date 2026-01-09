# Value Objects

## Overview

Value Objects are domain objects defined by their attributes rather than identity. They are immutable and can be freely shared and compared by value.

---

## 1. Transform Value Object

### Purpose
Represents a 3D transformation (position, rotation, scale) with immutable semantics.

```cpp
// Shared/ValueObjects/Transform.h
#pragma once

#include "CoreMinimal.h"

namespace Shared
{
    /**
     * Transform Value Object
     *
     * Immutable 3D transformation with location, rotation, and scale.
     * Wrapper around FTransform with value semantics.
     */
    struct FDomainTransform
    {
        // Construction
        FDomainTransform() = default;

        FDomainTransform(
            const FVector& InLocation,
            const FQuat& InRotation,
            const FVector& InScale = FVector::OneVector
        )
            : Location(InLocation)
            , Rotation(InRotation)
            , Scale(InScale)
        {}

        explicit FDomainTransform(const FTransform& UETransform)
            : Location(UETransform.GetLocation())
            , Rotation(UETransform.GetRotation())
            , Scale(UETransform.GetScale3D())
        {}

        // Accessors (no mutators - immutable)
        const FVector& GetLocation() const { return Location; }
        const FQuat& GetRotation() const { return Rotation; }
        const FVector& GetScale() const { return Scale; }

        FRotator GetRotator() const { return Rotation.Rotator(); }
        FVector GetForwardVector() const { return Rotation.GetForwardVector(); }
        FVector GetRightVector() const { return Rotation.GetRightVector(); }
        FVector GetUpVector() const { return Rotation.GetUpVector(); }

        // Transformation methods (return new instances)
        FDomainTransform WithLocation(const FVector& NewLocation) const
        {
            return FDomainTransform(NewLocation, Rotation, Scale);
        }

        FDomainTransform WithRotation(const FQuat& NewRotation) const
        {
            return FDomainTransform(Location, NewRotation, Scale);
        }

        FDomainTransform WithScale(const FVector& NewScale) const
        {
            return FDomainTransform(Location, Rotation, NewScale);
        }

        FDomainTransform Inverse() const
        {
            FQuat InvRotation = Rotation.Inverse();
            FVector InvScale = FVector(1.0f / Scale.X, 1.0f / Scale.Y, 1.0f / Scale.Z);
            FVector InvLocation = InvRotation.RotateVector(-Location) * InvScale;
            return FDomainTransform(InvLocation, InvRotation, InvScale);
        }

        FDomainTransform Compose(const FDomainTransform& Other) const
        {
            FVector NewLocation = Rotation.RotateVector(Other.Location * Scale) + Location;
            FQuat NewRotation = Rotation * Other.Rotation;
            FVector NewScale = Scale * Other.Scale;
            return FDomainTransform(NewLocation, NewRotation, NewScale);
        }

        // Point transformation
        FVector TransformPoint(const FVector& Point) const
        {
            return Rotation.RotateVector(Point * Scale) + Location;
        }

        FVector TransformVector(const FVector& Vector) const
        {
            return Rotation.RotateVector(Vector * Scale);
        }

        FVector InverseTransformPoint(const FVector& Point) const
        {
            return Rotation.UnrotateVector(Point - Location) / Scale;
        }

        // Matrix conversion
        FMatrix ToMatrix() const
        {
            FMatrix Result = FRotationMatrix(Rotation.Rotator());
            Result.SetOrigin(Location);
            Result.SetAxis(0, Result.GetScaledAxis(EAxis::X) * Scale.X);
            Result.SetAxis(1, Result.GetScaledAxis(EAxis::Y) * Scale.Y);
            Result.SetAxis(2, Result.GetScaledAxis(EAxis::Z) * Scale.Z);
            return Result;
        }

        // UE5 conversion
        FTransform ToUETransform() const
        {
            return FTransform(Rotation, Location, Scale);
        }

        // Value equality
        bool operator==(const FDomainTransform& Other) const
        {
            return Location.Equals(Other.Location, KINDA_SMALL_NUMBER)
                && Rotation.Equals(Other.Rotation, KINDA_SMALL_NUMBER)
                && Scale.Equals(Other.Scale, KINDA_SMALL_NUMBER);
        }

        bool operator!=(const FDomainTransform& Other) const
        {
            return !(*this == Other);
        }

        // Hash for use in containers
        friend uint32 GetTypeHash(const FDomainTransform& Transform)
        {
            return HashCombine(
                GetTypeHash(Transform.Location),
                HashCombine(
                    GetTypeHash(Transform.Rotation),
                    GetTypeHash(Transform.Scale)
                )
            );
        }

    private:
        FVector Location = FVector::ZeroVector;
        FQuat Rotation = FQuat::Identity;
        FVector Scale = FVector::OneVector;
    };
}
```

---

## 2. ProjectionMatrix Value Object

### Purpose
Represents camera projection parameters including intrinsic matrix and derived properties.

```cpp
// DEM/ValueObjects/DEM_ProjectionMatrix.h
#pragma once

#include "CoreMinimal.h"

namespace DEM
{
    /**
     * ProjectionMatrix Value Object
     *
     * Immutable camera projection parameters.
     * Supports both perspective and orthographic projections.
     */
    struct FProjectionMatrix
    {
        enum class EProjectionType : uint8
        {
            Perspective,
            Orthographic
        };

        // Factory methods (preferred construction)
        static FProjectionMatrix CreatePerspective(
            float Fx, float Fy,
            float Cx, float Cy,
            int32 Width, int32 Height
        )
        {
            FProjectionMatrix Result;
            Result.Type = EProjectionType::Perspective;
            Result.Fx = Fx;
            Result.Fy = Fy;
            Result.Cx = Cx;
            Result.Cy = Cy;
            Result.Width = Width;
            Result.Height = Height;
            Result.ComputeMatrix();
            return Result;
        }

        static FProjectionMatrix CreateFromFOV(
            float HorizontalFOV_Degrees,
            int32 Width, int32 Height
        )
        {
            float Fx = Width / (2.0f * FMath::Tan(FMath::DegreesToRadians(HorizontalFOV_Degrees) / 2.0f));
            float Fy = Fx;  // Square pixels
            float Cx = Width / 2.0f;
            float Cy = Height / 2.0f;
            return CreatePerspective(Fx, Fy, Cx, Cy, Width, Height);
        }

        static FProjectionMatrix CreateOrthographic(
            float Width_World, float Height_World,
            int32 ImageWidth, int32 ImageHeight
        )
        {
            FProjectionMatrix Result;
            Result.Type = EProjectionType::Orthographic;
            Result.Fx = ImageWidth / Width_World;
            Result.Fy = ImageHeight / Height_World;
            Result.Cx = ImageWidth / 2.0f;
            Result.Cy = ImageHeight / 2.0f;
            Result.Width = ImageWidth;
            Result.Height = ImageHeight;
            Result.ComputeMatrix();
            return Result;
        }

        // Accessors
        EProjectionType GetType() const { return Type; }
        bool IsPerspective() const { return Type == EProjectionType::Perspective; }

        float GetFx() const { return Fx; }
        float GetFy() const { return Fy; }
        float GetCx() const { return Cx; }
        float GetCy() const { return Cy; }

        FVector2D GetFocalLength() const { return FVector2D(Fx, Fy); }
        FVector2D GetPrincipalPoint() const { return FVector2D(Cx, Cy); }

        int32 GetWidth() const { return Width; }
        int32 GetHeight() const { return Height; }
        FIntPoint GetImageSize() const { return FIntPoint(Width, Height); }

        float GetAspectRatio() const { return static_cast<float>(Width) / Height; }

        float GetHorizontalFOV() const
        {
            if (!IsPerspective()) return 0.0f;
            return 2.0f * FMath::RadiansToDegrees(FMath::Atan(Width / (2.0f * Fx)));
        }

        float GetVerticalFOV() const
        {
            if (!IsPerspective()) return 0.0f;
            return 2.0f * FMath::RadiansToDegrees(FMath::Atan(Height / (2.0f * Fy)));
        }

        // Intrinsic matrix K (3x3)
        const FMatrix& GetIntrinsicMatrix() const { return K; }

        // Inverse intrinsic matrix
        FMatrix GetInverseIntrinsicMatrix() const
        {
            // K^-1 for pinhole camera
            FMatrix KInv = FMatrix::Identity;
            KInv.M[0][0] = 1.0f / Fx;
            KInv.M[1][1] = 1.0f / Fy;
            KInv.M[0][2] = -Cx / Fx;
            KInv.M[1][2] = -Cy / Fy;
            return KInv;
        }

        // Projection operations
        FVector2D Project(const FVector& CameraSpacePoint) const
        {
            if (IsPerspective())
            {
                float InvZ = 1.0f / CameraSpacePoint.Z;
                return FVector2D(
                    Fx * CameraSpacePoint.X * InvZ + Cx,
                    Fy * CameraSpacePoint.Y * InvZ + Cy
                );
            }
            else
            {
                return FVector2D(
                    Fx * CameraSpacePoint.X + Cx,
                    Fy * CameraSpacePoint.Y + Cy
                );
            }
        }

        FVector Unproject(const FVector2D& ImagePoint, float Depth) const
        {
            if (IsPerspective())
            {
                return FVector(
                    (ImagePoint.X - Cx) * Depth / Fx,
                    (ImagePoint.Y - Cy) * Depth / Fy,
                    Depth
                );
            }
            else
            {
                return FVector(
                    (ImagePoint.X - Cx) / Fx,
                    (ImagePoint.Y - Cy) / Fy,
                    Depth
                );
            }
        }

        // Ray from image point
        FVector GetRayDirection(const FVector2D& ImagePoint) const
        {
            FVector Dir = Unproject(ImagePoint, 1.0f);
            Dir.Normalize();
            return Dir;
        }

        // Value equality
        bool operator==(const FProjectionMatrix& Other) const
        {
            return Type == Other.Type
                && FMath::IsNearlyEqual(Fx, Other.Fx)
                && FMath::IsNearlyEqual(Fy, Other.Fy)
                && FMath::IsNearlyEqual(Cx, Other.Cx)
                && FMath::IsNearlyEqual(Cy, Other.Cy)
                && Width == Other.Width
                && Height == Other.Height;
        }

    private:
        void ComputeMatrix()
        {
            // 3x3 intrinsic matrix K
            K = FMatrix::Identity;
            K.M[0][0] = Fx;
            K.M[1][1] = Fy;
            K.M[0][2] = Cx;
            K.M[1][2] = Cy;
        }

        EProjectionType Type = EProjectionType::Perspective;
        float Fx = 1000.0f;
        float Fy = 1000.0f;
        float Cx = 960.0f;
        float Cy = 540.0f;
        int32 Width = 1920;
        int32 Height = 1080;
        FMatrix K;
    };
}
```

---

## 3. SHCoefficients Value Object

### Purpose
Represents Spherical Harmonics coefficients for view-dependent colour in 3DGS.

```cpp
// TRN/ValueObjects/TRN_SHCoefficients.h
#pragma once

#include "CoreMinimal.h"

namespace TRN
{
    /**
     * SHCoefficients Value Object
     *
     * Immutable Spherical Harmonics coefficients for view-dependent colour.
     * Supports degrees 0-3 (1, 4, 9, 16 basis functions).
     */
    struct FSHCoefficients
    {
        static constexpr int32 MAX_DEGREE = 3;
        static constexpr int32 COEFFS_PER_DEGREE[] = {1, 4, 9, 16};
        static constexpr int32 TOTAL_COEFFS_DEGREE_3 = 16;
        static constexpr int32 REST_SIZE = (TOTAL_COEFFS_DEGREE_3 - 1) * 3;  // 45

        // Factory methods
        static FSHCoefficients CreateFromRGB(const FLinearColor& Color)
        {
            FSHCoefficients Result;
            // DC component stores base colour (scaled by SH basis)
            // C0 = 0.28209479177387814 (SH basis for degree 0)
            constexpr float C0 = 0.28209479177387814f;
            Result.DC = FVector(Color.R, Color.G, Color.B) / C0;
            Result.Rest.SetNumZeroed(REST_SIZE);
            return Result;
        }

        static FSHCoefficients CreateFromDCAndRest(
            const FVector& InDC,
            const TArray<float>& InRest
        )
        {
            FSHCoefficients Result;
            Result.DC = InDC;
            Result.Rest = InRest;
            if (Result.Rest.Num() < REST_SIZE)
            {
                Result.Rest.SetNumZeroed(REST_SIZE);
            }
            return Result;
        }

        // Accessors
        const FVector& GetDC() const { return DC; }
        const TArray<float>& GetRest() const { return Rest; }

        float GetRed(int32 Index) const
        {
            if (Index == 0) return DC.X;
            int32 RestIndex = (Index - 1) * 3;
            return Rest.IsValidIndex(RestIndex) ? Rest[RestIndex] : 0.0f;
        }

        float GetGreen(int32 Index) const
        {
            if (Index == 0) return DC.Y;
            int32 RestIndex = (Index - 1) * 3 + 1;
            return Rest.IsValidIndex(RestIndex) ? Rest[RestIndex] : 0.0f;
        }

        float GetBlue(int32 Index) const
        {
            if (Index == 0) return DC.Z;
            int32 RestIndex = (Index - 1) * 3 + 2;
            return Rest.IsValidIndex(RestIndex) ? Rest[RestIndex] : 0.0f;
        }

        // Evaluate SH for view direction
        FLinearColor Evaluate(const FVector& ViewDirection) const
        {
            // SH basis functions (degree 0-3)
            constexpr float C0 = 0.28209479177387814f;
            constexpr float C1 = 0.4886025119029199f;
            constexpr float C2[] = {1.0925484305920792f, -1.0925484305920792f, 0.31539156525252005f, -1.0925484305920792f, 0.5462742152960396f};
            constexpr float C3[] = {-0.5900435899266435f, 2.890611442640554f, -0.4570457994644658f, 0.3731763325901154f, -0.4570457994644658f, 1.445305721320277f, -0.5900435899266435f};

            FVector Dir = ViewDirection.GetSafeNormal();
            float x = Dir.X, y = Dir.Y, z = Dir.Z;
            float xx = x*x, yy = y*y, zz = z*z;
            float xy = x*y, xz = x*z, yz = y*z;

            FVector Result = DC * C0;

            // Degree 1
            if (Rest.Num() >= 9)
            {
                Result.X += C1 * (Rest[0] * y + Rest[3] * z + Rest[6] * x);
                Result.Y += C1 * (Rest[1] * y + Rest[4] * z + Rest[7] * x);
                Result.Z += C1 * (Rest[2] * y + Rest[5] * z + Rest[8] * x);
            }

            // Degree 2 and 3 follow similar patterns...
            // (Simplified for brevity)

            return FLinearColor(
                FMath::Max(0.0f, Result.X),
                FMath::Max(0.0f, Result.Y),
                FMath::Max(0.0f, Result.Z),
                1.0f
            );
        }

        // Get base RGB colour (DC component)
        FLinearColor GetBaseColor() const
        {
            constexpr float C0 = 0.28209479177387814f;
            return FLinearColor(
                FMath::Max(0.0f, DC.X * C0),
                FMath::Max(0.0f, DC.Y * C0),
                FMath::Max(0.0f, DC.Z * C0),
                1.0f
            );
        }

        // Serialization
        TArray<float> ToFloatArray() const
        {
            TArray<float> Result;
            Result.Reserve(3 + REST_SIZE);
            Result.Add(DC.X);
            Result.Add(DC.Y);
            Result.Add(DC.Z);
            Result.Append(Rest);
            return Result;
        }

        static FSHCoefficients FromFloatArray(const TArray<float>& Data)
        {
            FSHCoefficients Result;
            if (Data.Num() >= 3)
            {
                Result.DC = FVector(Data[0], Data[1], Data[2]);
                if (Data.Num() > 3)
                {
                    Result.Rest.Reserve(Data.Num() - 3);
                    for (int32 i = 3; i < Data.Num(); ++i)
                    {
                        Result.Rest.Add(Data[i]);
                    }
                }
            }
            return Result;
        }

        // Value equality
        bool operator==(const FSHCoefficients& Other) const
        {
            if (!DC.Equals(Other.DC, KINDA_SMALL_NUMBER)) return false;
            if (Rest.Num() != Other.Rest.Num()) return false;
            for (int32 i = 0; i < Rest.Num(); ++i)
            {
                if (!FMath::IsNearlyEqual(Rest[i], Other.Rest[i])) return false;
            }
            return true;
        }

    private:
        FVector DC = FVector::ZeroVector;
        TArray<float> Rest;
    };
}
```

---

## 4. ColorBuffer Value Object

### Purpose
Represents an immutable image buffer with pixel data and format information.

```cpp
// DEM/ValueObjects/DEM_ColorBuffer.h
#pragma once

#include "CoreMinimal.h"

namespace DEM
{
    /**
     * Pixel format enumeration
     */
    enum class EBufferFormat : uint8
    {
        R8G8B8A8_SRGB,
        R8G8B8A8_Linear,
        R16G16B16A16_Float,
        R32G32B32A32_Float,
        R32_Float,      // Depth
        R8G8B8_SRGB,    // No alpha
    };

    /**
     * ColorBuffer Value Object
     *
     * Immutable image buffer with format metadata.
     */
    struct FColorBuffer
    {
        // Factory methods
        static FColorBuffer Create(
            int32 InWidth,
            int32 InHeight,
            EBufferFormat InFormat,
            TArray<uint8>&& InData
        )
        {
            FColorBuffer Result;
            Result.Width = InWidth;
            Result.Height = InHeight;
            Result.Format = InFormat;
            Result.Data = MoveTemp(InData);
            return Result;
        }

        static FColorBuffer CreateFromRenderTarget(
            UTextureRenderTarget2D* RenderTarget
        );

        // Accessors
        int32 GetWidth() const { return Width; }
        int32 GetHeight() const { return Height; }
        FIntPoint GetSize() const { return FIntPoint(Width, Height); }
        EBufferFormat GetFormat() const { return Format; }

        int64 GetDataSize() const { return Data.Num(); }
        const TArray<uint8>& GetData() const { return Data; }

        int32 GetBytesPerPixel() const
        {
            switch (Format)
            {
                case EBufferFormat::R8G8B8A8_SRGB:
                case EBufferFormat::R8G8B8A8_Linear:
                    return 4;
                case EBufferFormat::R16G16B16A16_Float:
                    return 8;
                case EBufferFormat::R32G32B32A32_Float:
                    return 16;
                case EBufferFormat::R32_Float:
                    return 4;
                case EBufferFormat::R8G8B8_SRGB:
                    return 3;
                default:
                    return 4;
            }
        }

        int32 GetRowStride() const { return Width * GetBytesPerPixel(); }

        // Pixel access (read-only)
        FLinearColor GetPixel(int32 X, int32 Y) const
        {
            if (X < 0 || X >= Width || Y < 0 || Y >= Height)
            {
                return FLinearColor::Black;
            }

            int32 Index = (Y * Width + X) * GetBytesPerPixel();

            switch (Format)
            {
                case EBufferFormat::R8G8B8A8_SRGB:
                case EBufferFormat::R8G8B8A8_Linear:
                {
                    return FLinearColor(
                        Data[Index] / 255.0f,
                        Data[Index + 1] / 255.0f,
                        Data[Index + 2] / 255.0f,
                        Data[Index + 3] / 255.0f
                    );
                }
                case EBufferFormat::R32G32B32A32_Float:
                {
                    const float* FloatData = reinterpret_cast<const float*>(&Data[Index]);
                    return FLinearColor(FloatData[0], FloatData[1], FloatData[2], FloatData[3]);
                }
                default:
                    return FLinearColor::Black;
            }
        }

        // Conversion
        FColorBuffer ConvertTo(EBufferFormat TargetFormat) const;

        // Image operations (return new buffers)
        FColorBuffer Resize(int32 NewWidth, int32 NewHeight) const;
        FColorBuffer Crop(int32 X, int32 Y, int32 CropWidth, int32 CropHeight) const;

        // Export
        bool SaveToPNG(const FString& FilePath) const;
        bool SaveToEXR(const FString& FilePath) const;

        // Validation
        bool IsValid() const
        {
            int64 ExpectedSize = static_cast<int64>(Width) * Height * GetBytesPerPixel();
            return Width > 0 && Height > 0 && Data.Num() == ExpectedSize;
        }

        // Value equality
        bool operator==(const FColorBuffer& Other) const
        {
            return Width == Other.Width
                && Height == Other.Height
                && Format == Other.Format
                && Data == Other.Data;
        }

    private:
        int32 Width = 0;
        int32 Height = 0;
        EBufferFormat Format = EBufferFormat::R8G8B8A8_SRGB;
        TArray<uint8> Data;
    };

    /**
     * DepthBuffer specialisation
     */
    struct FDepthBuffer
    {
        static FDepthBuffer Create(
            int32 InWidth,
            int32 InHeight,
            TArray<float>&& InData,
            float InNearPlane,
            float InFarPlane
        )
        {
            FDepthBuffer Result;
            Result.Width = InWidth;
            Result.Height = InHeight;
            Result.Data = MoveTemp(InData);
            Result.NearPlane = InNearPlane;
            Result.FarPlane = InFarPlane;
            return Result;
        }

        int32 GetWidth() const { return Width; }
        int32 GetHeight() const { return Height; }
        float GetNearPlane() const { return NearPlane; }
        float GetFarPlane() const { return FarPlane; }

        float GetDepth(int32 X, int32 Y) const
        {
            if (X < 0 || X >= Width || Y < 0 || Y >= Height) return 0.0f;
            return Data[Y * Width + X];
        }

        // Convert to normalised depth [0, 1]
        float GetNormalisedDepth(int32 X, int32 Y) const
        {
            float D = GetDepth(X, Y);
            return (D - NearPlane) / (FarPlane - NearPlane);
        }

        bool SaveToEXR(const FString& FilePath) const;

    private:
        int32 Width = 0;
        int32 Height = 0;
        TArray<float> Data;
        float NearPlane = 0.1f;
        float FarPlane = 10000.0f;
    };
}
```

---

## Value Object Design Principles

### Immutability

All value objects are immutable. Modifications create new instances:

```cpp
// Instead of mutation:
// transform.SetLocation(newLocation);  // WRONG

// Create new instance:
FDomainTransform newTransform = transform.WithLocation(newLocation);  // CORRECT
```

### Equality by Value

Value objects are compared by their attributes:

```cpp
FProjectionMatrix proj1 = FProjectionMatrix::CreateFromFOV(90.0f, 1920, 1080);
FProjectionMatrix proj2 = FProjectionMatrix::CreateFromFOV(90.0f, 1920, 1080);

// These are equal because they have the same attributes
check(proj1 == proj2);  // true
```

### Self-Validation

Value objects validate their own invariants:

```cpp
struct FValidatedProjection
{
    static TOptional<FValidatedProjection> Create(float FOV, int32 Width, int32 Height)
    {
        if (FOV <= 0.0f || FOV >= 180.0f) return {};
        if (Width <= 0 || Height <= 0) return {};

        return FValidatedProjection(FOV, Width, Height);
    }

private:
    FValidatedProjection(float FOV, int32 Width, int32 Height);
};
```
