# ADR-008: Anti-Aliasing Integration

## Status
Accepted

## Context

3D Gaussian Splatting can exhibit aliasing artifacts when:

1. Splats are smaller than pixel size at rendering distance
2. High-frequency details cause Moire patterns
3. View-dependent color changes rapidly with angle

Research advances address these issues:

| Technique | Paper | Improvement | Approach |
|-----------|-------|-------------|----------|
| Mip-Splatting | Yu et al. 2024 | +0.5-0.8 dB PSNR | 3D smoothing filter |
| Multi-Scale 3DGS | 2023 | +0.4 dB | Multiple scales per splat |
| Analytic-Splatting | 2024 | +0.3 dB | Analytical pixel integration |
| 2DGS | Huang et al. 2024 | +0.2 dB | 2D disk primitives |

The PRD lists anti-aliasing survey as research task R-006 and specifies integration considerations (Section 3.1, Priority Papers).

**Key insight**: Anti-aliasing primarily affects **training and rendering**, not export. However, export metadata can inform training pipelines about scene characteristics that affect anti-aliasing needs.

## Decision

We will implement **anti-aliasing awareness in the export manifest** and **optional capture-time settings**:

### Manifest Anti-Aliasing Flags

```json
{
    "version": "1.0",
    "generator": "UE5-3DGS",

    "anti_aliasing": {
        "capture_settings": {
            "method": "TAA",
            "samples": 8,
            "motion_blur": false,
            "dof": false
        },

        "scene_characteristics": {
            "has_fine_detail": true,
            "has_specular_surfaces": true,
            "has_thin_geometry": true,
            "estimated_min_feature_size_cm": 0.5
        },

        "training_recommendations": {
            "use_mip_splatting": true,
            "mip_splatting_filter_size": 0.3,
            "recommended_sh_bands": 3,
            "densification_grad_threshold": 0.0002,
            "recommended_iterations": 30000
        }
    }
}
```

### Scene Analysis for Anti-Aliasing

```cpp
class FSCM_SceneAnalyser
{
public:
    struct FSceneCharacteristics
    {
        bool bHasFineDetail = false;
        bool bHasSpecularSurfaces = false;
        bool bHasThinGeometry = false;
        float EstimatedMinFeatureSize_cm = 1.0f;
        float EstimatedMaxViewDistance_cm = 10000.0f;
        TArray<FString> MaterialTypes;
    };

    FSceneCharacteristics AnalyseScene(UWorld* World)
    {
        FSceneCharacteristics Result;

        // Analyse mesh complexity
        for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        {
            UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
            if (!Mesh) continue;

            // Check for thin geometry
            FBox Bounds = Mesh->Bounds.GetBox();
            FVector Size = Bounds.GetSize();
            float MinDimension = FMath::Min3(Size.X, Size.Y, Size.Z);

            if (MinDimension < 10.0f) // < 10cm
            {
                Result.bHasThinGeometry = true;
            }

            Result.EstimatedMinFeatureSize_cm = FMath::Min(
                Result.EstimatedMinFeatureSize_cm, MinDimension);

            // Analyse materials
            for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
            {
                UMaterialInterface* Material = Mesh->GetMaterial(i);
                if (Material)
                {
                    AnalyseMaterial(Material, Result);
                }
            }
        }

        return Result;
    }

private:
    void AnalyseMaterial(UMaterialInterface* Material, FSceneCharacteristics& Result)
    {
        // Check for specular/metallic
        float Metallic = 0.0f;
        float Roughness = 1.0f;

        if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material))
        {
            MID->GetScalarParameterValue(TEXT("Metallic"), Metallic);
            MID->GetScalarParameterValue(TEXT("Roughness"), Roughness);
        }

        if (Metallic > 0.5f || Roughness < 0.3f)
        {
            Result.bHasSpecularSurfaces = true;
            Result.MaterialTypes.AddUnique(TEXT("Specular"));
        }

        // Check for detail textures
        // ...
    }
};
```

### Capture Settings for Anti-Aliasing

```cpp
struct FSCM_CaptureConfig
{
    // Anti-aliasing for capture
    ESCM_AntiAliasingMethod AAMethod = ESCM_AntiAliasingMethod::TAA;
    int32 TAASamples = 8;
    bool bEnableMotionBlur = false;  // Must be OFF for 3DGS
    bool bEnableDoF = false;         // Must be OFF for sharp training

    // Additional clean capture settings
    bool bDisableBloom = true;
    bool bDisableLensFlare = true;
    bool bFixedExposure = true;
    float FixedExposureValue = 1.0f;
};

void FSCM_FrameRenderer::ConfigureAntiAliasing(USceneCaptureComponent2D* Capture)
{
    // Enable TAA for clean edges
    Capture->ShowFlags.SetTemporalAA(true);

    // Disable effects that corrupt training data
    Capture->PostProcessSettings.bOverride_MotionBlurAmount = true;
    Capture->PostProcessSettings.MotionBlurAmount = 0.0f;

    Capture->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
    Capture->PostProcessSettings.DepthOfFieldFocalDistance = 0.0f;

    // Fixed exposure for consistent lighting
    Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
    Capture->PostProcessSettings.AutoExposureBias = 0.0f;
}
```

### Training Recommendation Generation

```cpp
struct FSCM_TrainingRecommendations
{
    bool bUseMipSplatting = false;
    float MipSplattingFilterSize = 0.3f;
    int32 RecommendedSHBands = 3;
    float DensificationGradThreshold = 0.0002f;
    int32 RecommendedIterations = 30000;
    FString Notes;
};

FSCM_TrainingRecommendations GenerateRecommendations(
    const FSceneCharacteristics& Scene,
    const FSCM_CaptureConfig& Capture)
{
    FSCM_TrainingRecommendations Rec;

    // Mip-Splatting for scenes with fine detail or thin geometry
    if (Scene.bHasFineDetail || Scene.bHasThinGeometry)
    {
        Rec.bUseMipSplatting = true;
        Rec.MipSplattingFilterSize = FMath::Clamp(
            Scene.EstimatedMinFeatureSize_cm / Capture.Resolution.X * 100.0f,
            0.1f, 0.5f);
        Rec.Notes += TEXT("Mip-Splatting recommended for fine detail. ");
    }

    // Higher SH bands for specular surfaces
    if (Scene.bHasSpecularSurfaces)
    {
        Rec.RecommendedSHBands = 3;
        Rec.Notes += TEXT("Full SH bands (3) for specular materials. ");
    }
    else
    {
        Rec.RecommendedSHBands = 2;
    }

    // More iterations for complex scenes
    if (Scene.bHasFineDetail && Scene.bHasSpecularSurfaces)
    {
        Rec.RecommendedIterations = 40000;
        Rec.DensificationGradThreshold = 0.00015f;
    }

    return Rec;
}
```

### Manifest Output

```cpp
void FFCM_ManifestGenerator::WriteAntiAliasingSection(
    TSharedPtr<FJsonObject> Root,
    const FSceneCharacteristics& Scene,
    const FSCM_CaptureConfig& Capture,
    const FSCM_TrainingRecommendations& Recommendations)
{
    TSharedPtr<FJsonObject> AA = MakeShared<FJsonObject>();

    // Capture settings
    TSharedPtr<FJsonObject> CaptureSettings = MakeShared<FJsonObject>();
    CaptureSettings->SetStringField("method", "TAA");
    CaptureSettings->SetNumberField("samples", Capture.TAASamples);
    CaptureSettings->SetBoolField("motion_blur", false);
    CaptureSettings->SetBoolField("dof", false);
    AA->SetObjectField("capture_settings", CaptureSettings);

    // Scene characteristics
    TSharedPtr<FJsonObject> SceneChars = MakeShared<FJsonObject>();
    SceneChars->SetBoolField("has_fine_detail", Scene.bHasFineDetail);
    SceneChars->SetBoolField("has_specular_surfaces", Scene.bHasSpecularSurfaces);
    SceneChars->SetBoolField("has_thin_geometry", Scene.bHasThinGeometry);
    SceneChars->SetNumberField("estimated_min_feature_size_cm",
        Scene.EstimatedMinFeatureSize_cm);
    AA->SetObjectField("scene_characteristics", SceneChars);

    // Training recommendations
    TSharedPtr<FJsonObject> Training = MakeShared<FJsonObject>();
    Training->SetBoolField("use_mip_splatting", Recommendations.bUseMipSplatting);
    Training->SetNumberField("mip_splatting_filter_size",
        Recommendations.MipSplattingFilterSize);
    Training->SetNumberField("recommended_sh_bands", Recommendations.RecommendedSHBands);
    Training->SetNumberField("densification_grad_threshold",
        Recommendations.DensificationGradThreshold);
    Training->SetNumberField("recommended_iterations",
        Recommendations.RecommendedIterations);
    AA->SetObjectField("training_recommendations", Training);

    Root->SetObjectField("anti_aliasing", AA);
}
```

## Consequences

### Positive

- **Informed Training**: Users can configure Mip-Splatting based on scene analysis
- **Quality Improvement**: +0.5-0.8 dB PSNR with appropriate anti-aliasing
- **Clean Captures**: TAA without motion blur produces sharp training images
- **Automatic Analysis**: Scene characteristics detected without manual configuration
- **Documentation**: Manifest serves as record of capture settings

### Negative

- **Advisory Only**: Export cannot enforce training-time anti-aliasing
- **Analysis Overhead**: Scene analysis adds to export time
- **Heuristics**: Recommendations based on heuristics, not ground truth
- **External Dependency**: Mip-Splatting implementation is external to plugin

### Quality Impact

| Scenario | Without AA Awareness | With AA Awareness |
|----------|---------------------|-------------------|
| Fine foliage | Moire patterns | Clean edges |
| Thin geometry | Popping artifacts | Stable rendering |
| Specular surfaces | Banding | Smooth reflections |
| Distant objects | Aliased edges | Anti-aliased |

## Alternatives Considered

### 1. Built-in Mip-Splatting Training

**Description**: Implement Mip-Splatting in the optional training module

**Deferred to Phase 2**: Training module is optional; export-focused Phase 1

### 2. Supersampling Capture

**Description**: Capture at 4x resolution, downsample

**Rejected Because**:
- 16x memory increase
- Significantly slower capture
- TAA achieves similar results

### 3. Multiple Exposure Capture

**Description**: Capture each frame multiple times with jitter

**Rejected Because**:
- N-times slower
- Complex merge process
- TAA handles this natively

### 4. No Anti-Aliasing Consideration

**Description**: Export raw data, let training handle aliasing

**Rejected Because**:
- Users benefit from guidance
- Capture settings affect training quality
- Low-effort feature with high value

## References

- PRD Section 3.1: Research Agent Priority Papers (Mip-Splatting)
- PRD Task R-006: Survey anti-aliasing approaches in 3DGS
- Research: Rendering Advances (`docs/research/rendering-advances.md`)
- Mip-Splatting: https://arxiv.org/abs/2311.16493
- Multi-Scale 3DGS: https://arxiv.org/abs/2311.17089
- Analytic-Splatting: https://arxiv.org/abs/2403.11056
