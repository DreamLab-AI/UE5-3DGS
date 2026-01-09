# ADR-006: Depth Buffer Handling

## Status
Accepted

## Context

Unreal Engine 5 uses a **reverse-Z depth buffer** for improved floating-point precision:

| Depth System | Near Plane | Far Plane | Precision Distribution |
|--------------|------------|-----------|------------------------|
| Standard Z | Z = 0 | Z = 1 | More precision near |
| Reverse Z | Z = 1 | Z = 0 | More precision far |

The reverse-Z formula:
```
Z_buffer = Far / (Far - Near) - (Far * Near) / ((Far - Near) * Z_linear)
```

Simplified when Near approaches 0:
```
Z_buffer = Far / Z_linear  (for reverse-Z with infinite far plane)
```

Depth data is critical for:
1. **Depth-Supervised Training**: Adding depth loss improves PSNR by +0.8dB (see research)
2. **Mesh Initialization**: SuGaR and Mesh2Splat use depth for Gaussian placement
3. **Scale Estimation**: Metric depth enables correct scene scale

The PRD identifies depth buffer extraction as a core DEM responsibility (Section 4.2.2).

## Decision

We will implement a **multi-format depth extraction system** that:

1. Converts UE5 reverse-Z to linear depth in metres
2. Supports multiple output formats (EXR, PNG normalised, raw float)
3. Handles both finite and infinite far planes
4. Provides optional depth visualisation

### Depth Extraction Pipeline

```cpp
class FDEM_DepthExtractor : public IDEM_BufferExtractor
{
public:
    struct FDepthConfig
    {
        float NearPlane = 10.0f;        // UE5 default near (cm)
        float FarPlane = 100000.0f;     // UE5 default far (cm)
        bool bInfiniteFarPlane = true;  // UE5 uses infinite by default
        EDEM_DepthFormat OutputFormat = EDEM_DepthFormat::LinearMetres;
        EDEM_DepthRange NormalisationRange = EDEM_DepthRange::SceneBounds;
    };

    TSharedPtr<FFloatImage> ExtractDepthBuffer(
        UTextureRenderTarget2D* RenderTarget,
        const FDepthConfig& Config)
    {
        // 1. Read raw depth from render target
        TArray<FFloat16Color> RawDepth;
        FReadSurfaceDataFlags Flags;
        Flags.SetLinearToGamma(false);

        RenderTarget->GameThread_GetRenderTargetResource()
            ->ReadFloat16Pixels(RawDepth);

        // 2. Convert each pixel
        TSharedPtr<FFloatImage> LinearDepth = MakeShared<FFloatImage>();
        LinearDepth->Init(RenderTarget->SizeX, RenderTarget->SizeY);

        for (int32 i = 0; i < RawDepth.Num(); ++i)
        {
            float ReverseZ = RawDepth[i].R.GetFloat();
            float Linear = ConvertReverseZToLinear(ReverseZ, Config);

            // Convert to target format
            switch (Config.OutputFormat)
            {
                case EDEM_DepthFormat::LinearMetres:
                    LinearDepth->SetPixel(i, Linear / 100.0f); // cm -> m
                    break;
                case EDEM_DepthFormat::LinearCentimetres:
                    LinearDepth->SetPixel(i, Linear);
                    break;
                case EDEM_DepthFormat::Normalised_0_1:
                    LinearDepth->SetPixel(i, NormaliseDepth(Linear, Config));
                    break;
            }
        }

        return LinearDepth;
    }

private:
    float ConvertReverseZToLinear(float ReverseZ, const FDepthConfig& Config)
    {
        if (Config.bInfiniteFarPlane)
        {
            // UE5 infinite far plane formula
            // Z_buffer = Near / Z_linear
            return Config.NearPlane / FMath::Max(ReverseZ, SMALL_NUMBER);
        }
        else
        {
            // Finite far plane formula
            // Z_buffer = Far * Near / (Far - Z_linear * (Far - Near))
            // Solving for Z_linear:
            float Numerator = Config.FarPlane * Config.NearPlane;
            float Denominator = Config.FarPlane - ReverseZ * (Config.FarPlane - Config.NearPlane);
            return Numerator / FMath::Max(Denominator, SMALL_NUMBER);
        }
    }
};
```

### Depth Output Formats

```cpp
enum class EDEM_DepthFormat : uint8
{
    LinearMetres,       // Float32, metric depth (recommended for 3DGS)
    LinearCentimetres,  // Float32, UE5 native units
    Normalised_0_1,     // Float32, normalised to [0, 1]
    InverseDepth,       // Float32, 1/depth (for disparity)
    RawReverseZ         // Float32, unchanged buffer values
};

enum class EDEM_DepthRange : uint8
{
    SceneBounds,        // Normalise to scene bounding box
    CameraFrustum,      // Normalise to camera near/far
    FixedRange,         // User-specified min/max
    Auto                // Compute from depth values
};
```

### File Export

```cpp
void FFCM_DepthWriter::WriteDepthMaps(
    const FString& OutputPath,
    const TArray<TSharedPtr<FFloatImage>>& DepthMaps,
    const TArray<FString>& FrameNames)
{
    for (int32 i = 0; i < DepthMaps.Num(); ++i)
    {
        // Primary: 32-bit EXR (lossless float)
        FString EXRPath = OutputPath / TEXT("depth_maps") /
            FString::Printf(TEXT("%s.exr"), *FrameNames[i]);
        WriteEXR(EXRPath, DepthMaps[i]);

        // Secondary: 16-bit PNG (for visualisation)
        FString PNGPath = OutputPath / TEXT("depth_vis") /
            FString::Printf(TEXT("%s.png"), *FrameNames[i]);
        WriteNormalisedPNG(PNGPath, DepthMaps[i]);
    }
}
```

### Scene Capture Configuration

```cpp
void FSCM_FrameRenderer::ConfigureDepthCapture(USceneCaptureComponent2D* CaptureComponent)
{
    // Use Scene Depth for world-space distance
    CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;

    // High precision render target
    CaptureComponent->TextureTarget = NewObject<UTextureRenderTarget2D>();
    CaptureComponent->TextureTarget->InitCustomFormat(
        CaptureResolution.X,
        CaptureResolution.Y,
        PF_R32_FLOAT,  // 32-bit float precision
        false          // No gamma correction
    );

    // Disable post-processing that affects depth
    CaptureComponent->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
    CaptureComponent->PostProcessSettings.DepthOfFieldFocalDistance = 0.0f;
}
```

## Consequences

### Positive

- **Metric Accuracy**: Linear depth in metres matches real-world scale
- **Training Compatible**: Format matches depth-supervised 3DGS expectations
- **Lossless Storage**: EXR preserves full float32 precision
- **Multiple Use Cases**: Supports training, visualisation, and mesh initialisation
- **UE5 Native**: Uses SceneCapture for consistent results

### Negative

- **Storage Size**: Float32 EXR is 4x size of 8-bit PNG
- **Processing Overhead**: Per-pixel conversion adds frame export time
- **Precision Limits**: Very distant objects may have reduced precision

### Precision Analysis

| Distance (m) | Reverse-Z Value | Precision (cm) |
|--------------|-----------------|----------------|
| 0.1 | 0.9999 | 0.001 |
| 1.0 | 0.9990 | 0.01 |
| 10.0 | 0.9900 | 0.1 |
| 100.0 | 0.9000 | 1.0 |
| 1000.0 | 0.5000 | 10.0 |

## Alternatives Considered

### 1. Use Custom Depth Buffer

**Description**: Render depth via custom material/stencil

**Rejected Because**:
- Requires modifying scene materials
- Less accurate than native depth buffer
- Additional complexity

### 2. Store Raw Reverse-Z

**Description**: Export buffer values without conversion

**Rejected Because**:
- Incompatible with 3DGS training tools
- Non-metric values are harder to use
- Would require conversion elsewhere

### 3. Compute Depth from Reprojection

**Description**: Reconstruct depth from RGB via stereo or MVS

**Rejected Because**:
- Defeats purpose of ground-truth depth
- Introduces estimation errors
- Computationally expensive

### 4. 16-bit Depth Only

**Description**: Use 16-bit PNG for smaller files

**Rejected Because**:
- Insufficient precision for large scenes
- Quantisation artifacts at distance
- Training tools expect float depth

## References

- PRD Section 4.2.2: Data Extraction Module depth handling
- Research: Depth Supervision (`docs/research/camera-trajectories.md`, Depth Supervision section)
- UE5 Reverse-Z: https://docs.unrealengine.com/5.0/en-US/depth-buffer-precision-in-unreal-engine/
- DN-Splatter: https://arxiv.org/abs/2403.17822 (depth-supervised 3DGS)
