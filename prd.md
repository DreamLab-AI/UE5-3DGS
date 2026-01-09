# Product Requirements Document
## UE5-3DGS: In-Engine 3D Gaussian Splatting Pipeline for Unreal Engine 5

**Version:** 1.0  
**Date:** January 2025  
**Classification:** Technical Specification for Multi-Agent Development  
**Target Platform:** Unreal Engine 5.3+  
**Primary Language:** C++ (Plugin Architecture)

---

## 1. Executive Summary

### 1.1 Vision Statement

UE5-3DGS is an in-engine pipeline that enables real-time and batch generation of 3D Gaussian Splat representations from complete Unreal Engine 5 scenes. The system bridges the gap between traditional polygon-based real-time rendering and neural radiance field representations, enabling seamless export to industry-standard formats for use in research, production, and emerging spatial computing platforms.

### 1.2 Strategic Value Proposition

- **Novel Pipeline:** First comprehensive in-engine solution for synthetic 3DGS dataset generation from game-quality assets
- **Research Enablement:** Automated generation of ground-truth training data for 3DGS research with perfect camera pose information
- **Production Bridge:** Enable asset migration from UE5 to 3DGS-native platforms (VR viewers, web-based splat renderers, mobile AR)
- **Quality Benchmark:** Leverage UE5's rendering fidelity to establish new quality benchmarks for 3DGS reconstruction

### 1.3 Key Differentiators

| Capability | Current State-of-Art | UE5-3DGS Advantage |
|------------|---------------------|-------------------|
| Camera Pose Accuracy | COLMAP estimation (error-prone) | Perfect ground-truth from engine |
| Scene Complexity | Limited real-world capture | Unlimited synthetic complexity |
| Lighting Control | Environmental constraints | Full artistic control via Lumen |
| Depth Information | Estimated or LiDAR | Pixel-perfect depth buffer access |
| Reproducibility | Environmental variance | Deterministic rendering |

---

## 2. Problem Statement & Opportunity Analysis

### 2.1 Current Industry Pain Points

**For 3DGS Researchers:**
- Dependency on real-world captures with imperfect camera calibration
- Limited availability of high-quality training datasets
- No standardised synthetic data generation pipeline
- Difficulty isolating variables (lighting, materials, geometry) for ablation studies

**For Production Studios:**
- No pathway from existing UE5 assets to 3DGS format
- Manual reconstruction workflows requiring external photogrammetry
- Loss of material/lighting fidelity in conversion processes
- Inability to leverage existing virtual production pipelines

**For Spatial Computing Platforms:**
- Asset creation bottleneck for 3DGS-native experiences
- Quality gap between real captures and production requirements
- Lack of tooling for rapid iteration on splat-based content

### 2.2 Market Opportunity

The convergence of several technology vectors creates a compelling opportunity:

1. **3DGS Maturation:** Since Kerbl et al. (SIGGRAPH 2023), 3DGS has achieved real-time rendering at photorealistic quality
2. **Spatial Computing Growth:** Apple Vision Pro, Meta Quest, and emerging XR platforms increasingly support or require gaussian splat formats
3. **Virtual Production Adoption:** LED volume stages and real-time cinematography create demand for asset format flexibility
4. **AI Training Data Demand:** Synthetic data generation for computer vision and graphics ML models

---

## 3. Research Agent Specifications

The following research agents shall be deployed to maintain state-of-the-art awareness throughout development. Each agent uses Perplexity Pro for academic search with specific source targeting.

### 3.1 Agent: ARXIV_SCANNER

**Purpose:** Monitor and analyse emerging 3DGS research from arXiv

**Search Domains:**
- `arxiv.org/list/cs.CV` (Computer Vision)
- `arxiv.org/list/cs.GR` (Graphics)
- `arxiv.org/list/cs.LG` (Machine Learning)

**Query Templates:**
```
"3D Gaussian Splatting" AND (optimization OR training OR compression)
"neural radiance" AND "real-time" AND (2024 OR 2025)
"differentiable rendering" AND "point cloud" AND gaussian
"novel view synthesis" AND (speed OR efficiency OR quality)
"gaussian splat" AND (anti-aliasing OR LOD OR streaming)
```

**Output Schema:**
```json
{
  "paper_id": "arxiv:XXXX.XXXXX",
  "title": "string",
  "authors": ["string"],
  "publication_date": "YYYY-MM-DD",
  "relevance_score": 0.0-1.0,
  "key_contributions": ["string"],
  "implementation_available": boolean,
  "applicable_components": ["training", "export", "optimization", "rendering"],
  "integration_complexity": "low|medium|high",
  "summary": "string"
}
```

**Monitoring Frequency:** Daily scan, weekly synthesis report

**Priority Papers to Track:**
- 3DGS original (Kerbl et al., SIGGRAPH 2023)
- Mip-Splatting (Yu et al., 2024)
- 2D Gaussian Splatting (Huang et al., 2024)
- GaussianPro (Cheng et al., 2024)
- Scaffold-GS (Lu et al., 2024)
- 3DGS-DR (Ye et al., 2024) - Differentiable rendering improvements
- GOF (Yu et al., 2024) - Gaussian Opacity Fields
- GS-LRM (Zhang et al., 2024) - Large reconstruction models

### 3.2 Agent: GITHUB_TRACKER

**Purpose:** Monitor reference implementations, forks, and tooling

**Target Repositories:**
```
graphdeco-inria/gaussian-splatting (canonical)
graphdeco-inria/diff-gaussian-rasterization
nerfstudio-project/gsplat
antimatter15/splat
huggingface/gaussian-splatting
aras-p/UnityGaussianSplatting
```

**Search Queries:**
```
topic:gaussian-splatting language:cpp stars:>50
topic:3dgs topic:export
"colmap" "gaussian" "export" in:readme
"unreal" "gaussian" OR "3dgs" OR "splat"
"cuda" "gaussian" "splatting" "optimization"
```

**Output Schema:**
```json
{
  "repo_url": "string",
  "name": "string",
  "stars": int,
  "last_commit": "YYYY-MM-DD",
  "language_breakdown": {"cpp": 0.0, "python": 0.0, "cuda": 0.0},
  "key_features": ["string"],
  "license": "string",
  "integration_potential": "direct|partial|reference",
  "dependencies": ["string"],
  "build_system": "cmake|bazel|custom"
}
```

**Monitoring Frequency:** Bi-daily

### 3.3 Agent: HUGGINGFACE_MONITOR

**Purpose:** Track pre-trained models, datasets, and Spaces demos

**Search Scope:**
- Models: `task:image-to-3d`, `task:depth-estimation`, tags containing `gaussian`, `3dgs`, `splat`, `nerf`
- Datasets: `task:3d-reconstruction`, synthetic scene datasets
- Spaces: Interactive 3DGS demos and tools

**Query Templates:**
```
gaussian splatting
3dgs training
point cloud neural
synthetic scene dataset colmap
depth estimation mono
```

**Output Schema:**
```json
{
  "resource_type": "model|dataset|space",
  "hf_id": "org/name",
  "downloads_monthly": int,
  "license": "string",
  "applicable_use": "training|inference|evaluation|reference",
  "model_size": "string",
  "input_format": "string",
  "output_format": "string"
}
```

**Monitoring Frequency:** Weekly

### 3.4 Agent: STANDARDS_TRACKER

**Purpose:** Monitor format specifications and interoperability standards

**Target Standards:**
- COLMAP database schema and text format specifications
- PLY format variants for gaussian splats
- glTF extensions for point-based representations
- OpenXR and WebXR splat rendering proposals
- USD (Universal Scene Description) neural asset proposals

**Sources:**
- `colmap.github.io/format.html`
- `paulbourke.net/dataformats/ply/`
- `github.com/KhronosGroup/glTF`
- `github.com/PixarAnimationStudios/OpenUSD`

**Output Schema:**
```json
{
  "format_name": "string",
  "version": "string",
  "last_updated": "YYYY-MM-DD",
  "specification_url": "string",
  "supported_features": ["string"],
  "known_extensions": ["string"],
  "tool_support": ["string"]
}
```

---

## 4. Technical Architecture

### 4.1 High-Level System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           UE5-3DGS PIPELINE                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────────┐  │
│  │   SCENE CAPTURE  │───▶│  DATA EXTRACTION │───▶│  FORMAT CONVERSION   │  │
│  │     MODULE       │    │     MODULE       │    │      MODULE          │  │
│  └──────────────────┘    └──────────────────┘    └──────────────────────┘  │
│          │                       │                        │                │
│          ▼                       ▼                        ▼                │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────────┐  │
│  │ • Camera Path    │    │ • RGB Images     │    │ • COLMAP (txt/bin)   │  │
│  │   Generation     │    │ • Depth Maps     │    │ • PLY (splat)        │  │
│  │ • Trajectory     │    │ • Normal Maps    │    │ • Custom Binary      │  │
│  │   Optimisation   │    │ • Camera Params  │    │ • Training Manifest  │  │
│  │ • Coverage       │    │ • Scene Bounds   │    │                      │  │
│  │   Analysis       │    │ • Metadata       │    │                      │  │
│  └──────────────────┘    └──────────────────┘    └──────────────────────┘  │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                     OPTIONAL: IN-ENGINE TRAINING                      │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │  │
│  │  │ CUDA/GPU    │  │ Optimizer   │  │ Densifier   │  │ Rasterizer  │  │  │
│  │  │ Integration │  │ (Adam)      │  │ (Adaptive)  │  │ (Tile-based)│  │  │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘  │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Module Specifications

#### 4.2.1 Scene Capture Module (SCM)

**Responsibilities:**
- Generate optimal camera trajectories for scene coverage
- Execute high-fidelity rendering at configurable resolutions
- Extract per-frame metadata with nanosecond precision timestamps
- Support both offline batch and interactive capture modes

**Core Classes:**
```cpp
// Primary capture orchestrator
class FSCM_CaptureOrchestrator : public UObject
{
public:
    // Configuration
    void SetCaptureResolution(FIntPoint Resolution);
    void SetTrajectoryType(ESCM_TrajectoryType Type);
    void SetCoverageTarget(float TargetCoverage);
    
    // Execution
    TFuture<FSCM_CaptureResult> ExecuteCaptureAsync(UWorld* World);
    void PauseCaptureSession();
    void ResumeCaptureSession();
    
    // Progress
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCaptureProgress, int32, int32);
    FOnCaptureProgress OnCaptureProgress;
    
private:
    TSharedPtr<FSCM_TrajectoryGenerator> TrajectoryGenerator;
    TSharedPtr<FSCM_FrameRenderer> FrameRenderer;
    TSharedPtr<FSCM_CoverageAnalyser> CoverageAnalyser;
};

// Camera trajectory generation
enum class ESCM_TrajectoryType : uint8
{
    Spherical,          // Inward-facing capture (object-centric)
    Orbital,            // Orbit around scene centroid
    Flythrough,         // Path-based navigation
    Random_Stratified,  // Random with coverage constraints
    Grid_Systematic,    // Regular grid sampling
    Custom_Spline,      // User-defined spline path
    Adaptive_Coverage   // AI-driven coverage optimisation
};

// Frame data structure
struct FSCM_CapturedFrame
{
    int32 FrameIndex;
    double Timestamp;
    
    // Transform data (world space)
    FTransform CameraTransform;
    FMatrix ProjectionMatrix;
    FMatrix ViewMatrix;
    
    // Intrinsics
    float FocalLength_mm;
    float SensorWidth_mm;
    float SensorHeight_mm;
    FVector2D PrincipalPoint;
    TArray<float> DistortionCoefficients;
    
    // Rendered outputs
    TSharedPtr<FRenderTarget2D> ColorBuffer;
    TSharedPtr<FRenderTarget2D> DepthBuffer;
    TSharedPtr<FRenderTarget2D> NormalBuffer;
    TSharedPtr<FRenderTarget2D> MotionVectorBuffer;
    
    // Metadata
    TArray<FString> VisibleActors;
    FBox SceneBounds;
    float ExposureValue;
};
```

**Trajectory Generation Algorithms:**

1. **Spherical Sampling (Fibonacci Lattice)**
   - Optimal uniform distribution on sphere
   - Configurable radius and elevation constraints
   - Ideal for object-centric capture

2. **Adaptive Coverage (Research Priority)**
   - Neural coverage prediction using scene geometry
   - Iterative refinement based on reconstruction quality feedback
   - Integration with uncertainty estimation from 3DGS training

3. **Spline-Based Flythrough**
   - Catmull-Rom interpolation for smooth paths
   - Automatic keyframe generation from level streaming bounds
   - Support for cinematic camera rails

#### 4.2.2 Data Extraction Module (DEM)

**Responsibilities:**
- Extract RGB, depth, normals, and auxiliary buffers
- Convert engine-space coordinates to standard conventions
- Generate camera intrinsic/extrinsic parameters in multiple formats
- Compute scene statistics and bounds

**Key Interfaces:**
```cpp
// Buffer extraction interface
class IDEM_BufferExtractor
{
public:
    virtual TSharedPtr<FImage> ExtractColorBuffer(
        UTextureRenderTarget2D* RenderTarget,
        EDEM_ColorSpace ColorSpace = EDEM_ColorSpace::sRGB
    ) = 0;
    
    virtual TSharedPtr<FFloatImage> ExtractDepthBuffer(
        UTextureRenderTarget2D* RenderTarget,
        EDEM_DepthFormat Format = EDEM_DepthFormat::LinearMetres
    ) = 0;
    
    virtual TSharedPtr<FImage> ExtractNormalBuffer(
        UTextureRenderTarget2D* RenderTarget,
        EDEM_NormalSpace Space = EDEM_NormalSpace::WorldSpace
    ) = 0;
};

// Coordinate system conversion
enum class EDEM_CoordinateConvention : uint8
{
    UE5_LeftHanded_ZUp,    // Native UE5
    OpenCV_RightHanded,     // Standard CV (X-right, Y-down, Z-forward)
    OpenGL_RightHanded,     // OpenGL convention
    COLMAP_RightHanded,     // COLMAP native
    Blender_RightHanded_ZUp // Blender convention
};

// Camera parameter export
struct FDEM_CameraParameters
{
    // Extrinsics (world to camera transform)
    FMatrix WorldToCamera;
    FVector Position_World;
    FQuat Rotation_World;
    
    // Extrinsics in target convention
    FMatrix R;  // 3x3 rotation
    FVector t;  // Translation
    FQuat q;    // Quaternion (for COLMAP)
    
    // Intrinsics
    FMatrix K;  // 3x3 intrinsic matrix
    float fx, fy;  // Focal length in pixels
    float cx, cy;  // Principal point
    
    // Distortion (OpenCV model)
    float k1, k2, k3;  // Radial
    float p1, p2;      // Tangential
    
    // Metadata
    int32 ImageWidth;
    int32 ImageHeight;
    FString CameraModel;  // "PINHOLE", "OPENCV", "OPENCV_FISHEYE"
};
```

**Depth Buffer Handling:**

The depth buffer extraction requires careful handling due to UE5's reverse-Z depth buffer:

```cpp
// Convert UE5 reverse-Z depth to linear depth in metres
float ConvertReverseZToLinear(float ReverseZ, float NearPlane, float FarPlane)
{
    // UE5 uses reverse-Z: Z_buffer = (Far * Near) / (Far - Z_linear * (Far - Near))
    // Invert to get linear depth
    float LinearDepth = (FarPlane * NearPlane) / 
                        (FarPlane - ReverseZ * (FarPlane - NearPlane));
    return LinearDepth;
}

// Alternative: Use Scene Depth directly via Custom Depth Stencil
// This provides world-space distance from camera
```

#### 4.2.3 Format Conversion Module (FCM)

**Responsibilities:**
- Export to COLMAP database and text formats
- Generate 3DGS-compatible PLY files
- Create training configuration manifests
- Support custom binary formats for streaming

**COLMAP Export Specification:**

```cpp
// COLMAP text format writer
class FFCM_COLMAPWriter
{
public:
    // Write complete COLMAP dataset
    void WriteDataset(
        const FString& OutputPath,
        const TArray<FDEM_CameraParameters>& Cameras,
        const TArray<FString>& ImagePaths,
        bool bWriteBinary = true
    );
    
private:
    // cameras.txt / cameras.bin
    void WriteCamerasFile(
        const FString& Path,
        const TArray<FDEM_CameraParameters>& Cameras,
        bool bBinary
    );
    
    // images.txt / images.bin
    void WriteImagesFile(
        const FString& Path,
        const TArray<FDEM_CameraParameters>& Cameras,
        const TArray<FString>& ImagePaths,
        bool bBinary
    );
    
    // points3D.txt / points3D.bin (optional - can be empty for 3DGS)
    void WritePointsFile(
        const FString& Path,
        const TArray<FVector>& Points,
        const TArray<FColor>& Colors,
        bool bBinary
    );
};

// COLMAP cameras.txt format
// CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]
// Example: 1 PINHOLE 1920 1080 1597.23 1597.23 960.0 540.0

// COLMAP images.txt format  
// IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME
// POINTS2D[] as (X, Y, POINT3D_ID) - can be empty
```

**PLY Export (3DGS Format):**

```cpp
// 3DGS-compatible PLY structure
struct F3DGS_Splat
{
    // Position (3 floats)
    FVector Position;
    
    // Normal (3 floats) - often unused but required by format
    FVector Normal;
    
    // Spherical Harmonics coefficients
    // DC component (RGB) - 3 floats
    FVector SH_DC;
    // Higher bands - 45 floats (15 per channel for degree 3)
    float SH_Rest[45];
    
    // Opacity (1 float, pre-sigmoid)
    float Opacity;
    
    // Scale (3 floats, log-space)
    FVector Scale;
    
    // Rotation (4 floats, quaternion)
    FQuat Rotation;
};

// PLY header for 3DGS
/*
ply
format binary_little_endian 1.0
element vertex <count>
property float x
property float y
property float z
property float nx
property float ny
property float nz
property float f_dc_0
property float f_dc_1
property float f_dc_2
property float f_rest_0
... (f_rest_1 through f_rest_44)
property float opacity
property float scale_0
property float scale_1
property float scale_2
property float rot_0
property float rot_1
property float rot_2
property float rot_3
end_header
*/
```

### 4.3 Optional In-Engine Training Module

**Note:** This module is marked as optional for Phase 2. Initial release focuses on export for external training.

**Architecture Overview:**

```cpp
// GPU training integration via CUDA
class F3DGS_TrainingModule
{
public:
    // Initialisation
    bool InitialiseCUDA();
    bool LoadTrainingData(const FString& COLMAPPath);
    
    // Training control
    void StartTraining(const F3DGS_TrainingConfig& Config);
    void PauseTraining();
    void ResumeTraining();
    void StopTraining();
    
    // Iteration hooks
    DECLARE_DELEGATE_TwoParams(FOnIterationComplete, int32, F3DGS_TrainingMetrics);
    FOnIterationComplete OnIterationComplete;
    
    // Export trained model
    void ExportPLY(const FString& OutputPath);
    
private:
    // CUDA resources
    void* CudaContext;
    void* CudaStream;
    
    // Gaussian parameters (GPU memory)
    void* d_Positions;
    void* d_SH_Coefficients;
    void* d_Opacities;
    void* d_Scales;
    void* d_Rotations;
    
    // Adam optimizer state
    void* d_Moments1;
    void* d_Moments2;
};

struct F3DGS_TrainingConfig
{
    // Iterations
    int32 TotalIterations = 30000;
    int32 WarmupIterations = 500;
    
    // Learning rates
    float PositionLR_Initial = 0.00016f;
    float PositionLR_Final = 0.0000016f;
    float FeatureLR = 0.0025f;
    float OpacityLR = 0.05f;
    float ScalingLR = 0.005f;
    float RotationLR = 0.001f;
    
    // Densification
    int32 DensifyFromIter = 500;
    int32 DensifyUntilIter = 15000;
    int32 DensificationInterval = 100;
    float DensifyGradThreshold = 0.0002f;
    
    // Pruning
    float OpacityPruneThreshold = 0.005f;
    float ScalePruneThreshold = 0.1f;  // Relative to scene extent
    
    // Loss weights
    float LambdaL1 = 0.8f;
    float LambdaSSIM = 0.2f;
};
```

---

## 5. UE5 Integration Architecture

### 5.1 Plugin Structure

```
UE5_3DGS_Plugin/
├── Source/
│   ├── UE5_3DGS/
│   │   ├── Public/
│   │   │   ├── UE5_3DGS.h
│   │   │   ├── SCM/                    # Scene Capture Module
│   │   │   │   ├── SCM_CaptureOrchestrator.h
│   │   │   │   ├── SCM_TrajectoryGenerator.h
│   │   │   │   ├── SCM_FrameRenderer.h
│   │   │   │   └── SCM_CoverageAnalyser.h
│   │   │   ├── DEM/                    # Data Extraction Module
│   │   │   │   ├── DEM_BufferExtractor.h
│   │   │   │   ├── DEM_CoordinateConverter.h
│   │   │   │   └── DEM_CameraParameters.h
│   │   │   ├── FCM/                    # Format Conversion Module
│   │   │   │   ├── FCM_COLMAPWriter.h
│   │   │   │   ├── FCM_PLYWriter.h
│   │   │   │   └── FCM_ManifestGenerator.h
│   │   │   ├── TRN/                    # Training Module (Optional)
│   │   │   │   ├── TRN_CUDAIntegration.h
│   │   │   │   ├── TRN_Optimizer.h
│   │   │   │   └── TRN_Rasterizer.h
│   │   │   └── UI/
│   │   │       ├── UI_CaptureWidget.h
│   │   │       └── UI_ProgressPanel.h
│   │   ├── Private/
│   │   │   └── [Implementation files]
│   │   └── UE5_3DGS.Build.cs
│   └── UE5_3DGS_Editor/
│       ├── Public/
│       │   ├── UE5_3DGS_EditorMode.h
│       │   └── UE5_3DGS_Toolbar.h
│       └── Private/
│           └── [Editor implementation]
├── Shaders/
│   └── Private/
│       ├── DepthExtraction.usf
│       └── NormalExtraction.usf
├── Content/
│   ├── UI/
│   │   └── WBP_CapturePanel.uasset
│   └── Materials/
│       └── M_DepthVisualisation.uasset
├── ThirdParty/
│   ├── CUDA/
│   │   └── [CUDA toolkit headers/libs]
│   └── Eigen/
│       └── [Eigen header-only library]
├── Resources/
│   └── Icon128.png
├── Config/
│   └── DefaultUE5_3DGS.ini
└── UE5_3DGS.uplugin
```

### 5.2 Build Configuration

```cs
// UE5_3DGS.Build.cs
using UnrealBuildTool;

public class UE5_3DGS : ModuleRules
{
    public UE5_3DGS(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "RHI",
            "Renderer",
            "Projects"
        });
        
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore",
            "ImageWrapper",
            "ImageWriteQueue",
            "MovieSceneCapture"
        });
        
        // CUDA integration (optional, for training module)
        if (Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.Linux)
        {
            string CUDAPath = Environment.GetEnvironmentVariable("CUDA_PATH");
            if (!string.IsNullOrEmpty(CUDAPath))
            {
                PublicIncludePaths.Add(Path.Combine(CUDAPath, "include"));
                PublicAdditionalLibraries.Add(
                    Path.Combine(CUDAPath, "lib", "x64", "cuda.lib")
                );
                PublicDefinitions.Add("WITH_CUDA=1");
            }
        }
        
        // Eigen for linear algebra
        PublicIncludePaths.Add(
            Path.Combine(ModuleDirectory, "..", "ThirdParty", "Eigen")
        );
    }
}
```

### 5.3 Editor Integration

**Custom Editor Mode:**

```cpp
// UE5_3DGS_EditorMode.h
class FUE5_3DGS_EditorMode : public FEdMode
{
public:
    const static FEditorModeID EM_3DGS_Capture;
    
    // FEdMode interface
    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
    virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
    
    // Visualisation
    void DrawCameraFrustums(FPrimitiveDrawInterface* PDI);
    void DrawCoverageHeatmap(FPrimitiveDrawInterface* PDI);
    void DrawTrajectoryPath(FPrimitiveDrawInterface* PDI);
    
private:
    TSharedPtr<FSCM_CaptureOrchestrator> CaptureOrchestrator;
    TArray<FTransform> PlannedCameraPositions;
    bool bShowCoverageOverlay = false;
};
```

**Toolbar Extension:**

```cpp
// Adds "3DGS Capture" button to Level Editor toolbar
void FUE5_3DGS_EditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);
    
    UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(
        "LevelEditor.LevelEditorToolBar.PlayToolBar"
    );
    
    FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("3DGS");
    Section.AddEntry(FToolMenuEntry::InitToolBarButton(
        FUE5_3DGS_Commands::Get().OpenCapturePanel,
        LOCTEXT("3DGSCapture", "3DGS Capture"),
        LOCTEXT("3DGSCaptureTooltip", "Open 3D Gaussian Splatting capture panel"),
        FSlateIcon(FUE5_3DGS_Style::GetStyleSetName(), "3DGS.CaptureIcon")
    ));
}
```

---

## 6. Export Format Specifications

### 6.1 COLMAP Format (Primary)

**Directory Structure:**
```
output/
├── images/
│   ├── frame_00000.png
│   ├── frame_00001.png
│   └── ...
├── sparse/
│   └── 0/
│       ├── cameras.bin (or .txt)
│       ├── images.bin (or .txt)
│       └── points3D.bin (or .txt)
└── database.db (optional)
```

**cameras.bin Format:**
```
num_cameras (uint64)
for each camera:
    camera_id (uint32)
    model_id (int32)  // 0=SIMPLE_PINHOLE, 1=PINHOLE, 4=OPENCV, etc.
    width (uint64)
    height (uint64)
    params (double[]) // Depends on model
```

**images.bin Format:**
```
num_images (uint64)
for each image:
    image_id (uint32)
    qw, qx, qy, qz (double[4])  // Rotation quaternion (world-to-camera)
    tx, ty, tz (double[3])      // Translation (world-to-camera)
    camera_id (uint32)
    name (string, null-terminated)
    num_points2D (uint64)
    for each point2D:
        x, y (double[2])
        point3D_id (int64)  // -1 if not triangulated
```

**Coordinate Convention:**
- COLMAP uses a right-handed coordinate system
- X: right, Y: down, Z: forward (camera looking along +Z)
- Quaternion is scalar-first: (w, x, y, z)
- Transform is world-to-camera (inverse of camera-to-world)

### 6.2 PLY Format (3DGS Native)

**Minimal PLY Header:**
```
ply
format binary_little_endian 1.0
element vertex <N>
property float x
property float y
property float z
property float f_dc_0
property float f_dc_1
property float f_dc_2
property float opacity
property float scale_0
property float scale_1
property float scale_2
property float rot_0
property float rot_1
property float rot_2
property float rot_3
end_header
<binary data>
```

**Extended PLY (with SH coefficients):**
```
ply
format binary_little_endian 1.0
element vertex <N>
property float x
property float y
property float z
property float nx
property float ny
property float nz
property float f_dc_0
property float f_dc_1
property float f_dc_2
property float f_rest_0
... (through f_rest_44)
property float opacity
property float scale_0
property float scale_1
property float scale_2
property float rot_0
property float rot_1
property float rot_2
property float rot_3
end_header
<binary data>
```

### 6.3 Training Manifest (Custom)

**JSON Schema:**
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "UE5-3DGS Training Manifest",
  "type": "object",
  "properties": {
    "version": { "type": "string", "const": "1.0" },
    "generator": { "type": "string", "const": "UE5-3DGS" },
    "generation_timestamp": { "type": "string", "format": "date-time" },
    
    "scene_info": {
      "type": "object",
      "properties": {
        "name": { "type": "string" },
        "ue5_project": { "type": "string" },
        "ue5_level": { "type": "string" },
        "bounds_min": { "type": "array", "items": { "type": "number" }, "minItems": 3, "maxItems": 3 },
        "bounds_max": { "type": "array", "items": { "type": "number" }, "minItems": 3, "maxItems": 3 },
        "up_axis": { "type": "string", "enum": ["Y", "Z"] },
        "scale_metres": { "type": "number" }
      }
    },
    
    "capture_config": {
      "type": "object",
      "properties": {
        "trajectory_type": { "type": "string" },
        "num_frames": { "type": "integer" },
        "resolution": { "type": "array", "items": { "type": "integer" }, "minItems": 2, "maxItems": 2 },
        "fov_degrees": { "type": "number" },
        "near_plane": { "type": "number" },
        "far_plane": { "type": "number" }
      }
    },
    
    "render_settings": {
      "type": "object",
      "properties": {
        "lighting_mode": { "type": "string", "enum": ["Lumen", "PathTracing", "Baked"] },
        "anti_aliasing": { "type": "string" },
        "motion_blur": { "type": "boolean" },
        "exposure_mode": { "type": "string" }
      }
    },
    
    "output_paths": {
      "type": "object",
      "properties": {
        "images": { "type": "string" },
        "colmap_sparse": { "type": "string" },
        "depth_maps": { "type": "string" },
        "normal_maps": { "type": "string" }
      }
    },
    
    "ground_truth": {
      "type": "object",
      "properties": {
        "depth_available": { "type": "boolean" },
        "depth_format": { "type": "string" },
        "depth_units": { "type": "string" },
        "normals_available": { "type": "boolean" },
        "normals_space": { "type": "string" }
      }
    },
    
    "recommended_training": {
      "type": "object",
      "properties": {
        "iterations": { "type": "integer" },
        "position_lr_init": { "type": "number" },
        "densify_from_iter": { "type": "integer" },
        "densify_until_iter": { "type": "integer" },
        "densification_interval": { "type": "integer" }
      }
    }
  },
  "required": ["version", "scene_info", "capture_config", "output_paths"]
}
```

---

## 7. Quality Assurance & Validation

### 7.1 Automated Test Suite

**Unit Tests:**
```cpp
// Test coordinate conversion accuracy
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCoordinateConversionTest,
    "UE5_3DGS.DEM.CoordinateConversion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCoordinateConversionTest::RunTest(const FString& Parameters)
{
    // Test UE5 -> COLMAP -> UE5 round-trip
    FTransform OriginalTransform(
        FQuat(FVector::UpVector, PI / 4),
        FVector(100, 200, 300)
    );
    
    FDEM_CameraParameters COLMAPParams = ConvertToCOLMAP(OriginalTransform);
    FTransform RoundTripped = ConvertFromCOLMAP(COLMAPParams);
    
    TestTrue("Position preserved", 
        OriginalTransform.GetLocation().Equals(RoundTripped.GetLocation(), 0.001f));
    TestTrue("Rotation preserved",
        OriginalTransform.GetRotation().Equals(RoundTripped.GetRotation(), 0.0001f));
    
    return true;
}
```

**Integration Tests:**
```cpp
// Test full pipeline on reference scene
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FFullPipelineTest,
    "UE5_3DGS.Integration.FullPipeline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

void FFullPipelineTest::GetTests(TArray<FString>& OutBeautifiedNames, 
                                  TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add("Capture_Export_Validate");
    OutTestCommands.Add("/Game/3DGS_TestLevel");
}

bool FFullPipelineTest::RunTest(const FString& Parameters)
{
    // 1. Load test level
    // 2. Execute capture with known parameters
    // 3. Export to COLMAP format
    // 4. Validate output files exist and are well-formed
    // 5. Run external 3DGS training (if available)
    // 6. Compare reconstruction metrics against baseline
    return true;
}
```

### 7.2 Validation Metrics

**Capture Quality Metrics:**
- Scene coverage percentage (target: >95%)
- Camera baseline distribution (should follow log-normal)
- Angular coverage uniformity (spherical standard deviation)
- Depth range utilisation

**Export Validation:**
- COLMAP format parser acceptance
- Camera parameter consistency check
- Image-pose alignment verification (reprojection of known 3D points)

**Reconstruction Quality (when training module active):**
- PSNR against held-out views (target: >30dB)
- SSIM against held-out views (target: >0.95)
- LPIPS perceptual metric
- Training convergence rate

---

## 8. Agent Swarm Task Decomposition

This section defines discrete, parallelisable work units for multi-agent development.

### 8.1 Research Phase Tasks

| Task ID | Task Name | Agent Type | Dependencies | Output |
|---------|-----------|------------|--------------|--------|
| R-001 | Survey 3DGS training implementations | ARXIV_SCANNER + GITHUB_TRACKER | None | Implementation comparison matrix |
| R-002 | Analyse COLMAP format edge cases | STANDARDS_TRACKER | None | Format compliance checklist |
| R-003 | Benchmark coordinate conversion approaches | Research | None | Accuracy/performance analysis |
| R-004 | Survey camera trajectory optimization literature | ARXIV_SCANNER | None | Algorithm recommendation report |
| R-005 | Analyse UE5 Movie Render Queue capabilities | Development | None | Feature gap analysis |
| R-006 | Survey anti-aliasing approaches in 3DGS | ARXIV_SCANNER | None | Integration recommendations |
| R-007 | Evaluate CUDA-UE5 integration patterns | GITHUB_TRACKER | None | Architecture decision record |

### 8.2 Development Phase Tasks

| Task ID | Task Name | Agent Type | Dependencies | Output |
|---------|-----------|------------|--------------|--------|
| D-001 | Implement basic plugin scaffold | CPP_DEV | None | UE5 plugin compiling |
| D-002 | Implement coordinate conversion utilities | CPP_DEV | R-003 | FDEM_CoordinateConverter class |
| D-003 | Implement spherical trajectory generator | CPP_DEV | R-004 | FSCM_SphericalTrajectory class |
| D-004 | Implement frame capture orchestration | CPP_DEV | D-001 | FSCM_CaptureOrchestrator class |
| D-005 | Implement depth buffer extraction | CPP_DEV | D-001 | FDEM_DepthExtractor class |
| D-006 | Implement COLMAP text format writer | CPP_DEV | D-002, R-002 | FFCM_COLMAPTextWriter class |
| D-007 | Implement COLMAP binary format writer | CPP_DEV | D-006 | FFCM_COLMAPBinaryWriter class |
| D-008 | Implement PLY export (minimal) | CPP_DEV | D-002 | FFCM_PLYWriter class |
| D-009 | Implement editor mode visualization | CPP_DEV | D-003 | Editor mode with trajectory preview |
| D-010 | Implement capture progress UI | CPP_DEV | D-004 | Slate widget for progress |
| D-011 | Implement coverage analysis | CPP_DEV | D-003 | FSCM_CoverageAnalyser class |
| D-012 | Implement adaptive trajectory generator | CPP_DEV | D-011, R-004 | FSCM_AdaptiveTrajectory class |
| D-013 | Implement manifest generator | CPP_DEV | D-004, D-006 | FFCM_ManifestGenerator class |
| D-014 | Implement normal buffer extraction | CPP_DEV | D-005 | FDEM_NormalExtractor class |
| D-015 | Integrate CUDA runtime (optional) | CPP_DEV | R-007 | CUDA context management |
| D-016 | Implement 3DGS training core (optional) | CPP_DEV + CUDA_DEV | D-015 | F3DGS_TrainingModule class |

### 8.3 Testing Phase Tasks

| Task ID | Task Name | Agent Type | Dependencies | Output |
|---------|-----------|------------|--------------|--------|
| T-001 | Create unit test suite | TEST_DEV | D-002, D-003 | Automated unit tests |
| T-002 | Create reference test scenes | CONTENT_DEV | None | UE5 test levels |
| T-003 | Validate COLMAP export compatibility | TEST_DEV | D-006, D-007, T-002 | Compatibility report |
| T-004 | Benchmark capture performance | TEST_DEV | D-004, T-002 | Performance metrics |
| T-005 | Validate 3DGS training integration | TEST_DEV | D-016, T-002 | Training validation report |

### 8.4 Documentation Phase Tasks

| Task ID | Task Name | Agent Type | Dependencies | Output |
|---------|-----------|------------|--------------|--------|
| DOC-001 | Write API reference documentation | DOC_DEV | D-* | Doxygen/Blueprint docs |
| DOC-002 | Create quickstart tutorial | DOC_DEV | D-004, D-006 | Tutorial document |
| DOC-003 | Document best practices for capture | DOC_DEV | T-004 | Best practices guide |
| DOC-004 | Create troubleshooting guide | DOC_DEV | T-003 | FAQ document |

---

## 9. Risk Assessment & Mitigation

### 9.1 Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| UE5 render pipeline changes break depth extraction | Medium | High | Abstract buffer extraction; version-specific paths |
| CUDA version conflicts with UE5 dependencies | Medium | Medium | Isolate CUDA code; make training module optional |
| Coordinate system bugs produce invalid exports | High | High | Comprehensive unit tests; round-trip validation |
| Performance bottleneck in high-res capture | Medium | Medium | Async rendering; frame batching; resolution presets |
| 3DGS format evolution breaks compatibility | Low | Medium | Modular export system; version detection |

### 9.2 Research Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| New 3DGS variants require architecture changes | Medium | Medium | Modular training module; plugin architecture |
| Superior synthetic data methods emerge | Low | High | Research agents provide early warning |
| Licensing issues with reference implementations | Low | High | Clean-room implementation; MIT/Apache preference |

### 9.3 Project Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Scope creep from in-engine training | High | High | Training module explicitly Phase 2 |
| UE5 version fragmentation | Medium | Medium | Support matrix; minimum version 5.3 |
| Agent coordination overhead | Medium | Medium | Clear task boundaries; defined interfaces |

---

## 10. Implementation Roadmap

### Phase 1: Core Export Pipeline (8 weeks)

**Week 1-2: Foundation**
- Plugin scaffold and build system
- Coordinate conversion utilities with full test coverage
- Basic spherical trajectory generator

**Week 3-4: Capture System**
- Frame capture orchestration
- RGB/Depth/Normal buffer extraction
- Movie Render Queue integration exploration

**Week 5-6: Export System**
- COLMAP text format export
- COLMAP binary format export
- Basic PLY export (positions only)

**Week 7-8: Polish & Testing**
- Editor mode visualization
- Progress UI
- Integration testing with external 3DGS training

**Deliverable:** Functional UE5 plugin that exports scenes to COLMAP format for training with `gaussian-splatting` reference implementation.

### Phase 2: Enhanced Features (6 weeks)

**Week 9-10: Advanced Trajectories**
- Adaptive coverage trajectory generator
- Coverage analysis and visualization
- Spline-based custom paths

**Week 11-12: Extended Export**
- Full PLY export with SH initialization
- Manifest generator with recommended training params
- Multiple coordinate convention support

**Week 13-14: Optional Training**
- CUDA integration scaffold
- Proof-of-concept in-engine training
- Training progress visualization

**Deliverable:** Feature-complete plugin with advanced trajectory options and optional in-engine training.

### Phase 3: Production Hardening (4 weeks)

**Week 15-16: Performance**
- Async capture pipeline
- Memory optimization for large scenes
- Batch processing for multi-scene export

**Week 17-18: Documentation & Release**
- Complete API documentation
- Tutorial content
- Marketplace preparation (if applicable)

**Deliverable:** Production-ready release candidate.

---

## 11. Success Metrics

### 11.1 Technical Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Export accuracy (position) | <1mm error | Round-trip test vs. known transforms |
| Export accuracy (rotation) | <0.01° error | Quaternion comparison test |
| Capture throughput | >5 fps @ 1080p | Benchmark on reference hardware |
| COLMAP compatibility | 100% acceptance | Validation with COLMAP CLI |
| 3DGS training success rate | >95% | Automated training test suite |
| Reconstruction PSNR | >30dB | Holdout view evaluation |

### 11.2 Project Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Code coverage | >80% | Automated coverage reports |
| Documentation coverage | 100% public API | Doxygen warnings |
| Build time | <5 minutes | CI metrics |
| Plugin size | <50MB | Release build size |

### 11.3 User Metrics (Post-Release)

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Successful exports | >90% first attempt | Telemetry (opt-in) |
| Support tickets | <10/month | Issue tracker |
| Community adoption | >1000 downloads (month 1) | Marketplace/GitHub stats |

---

## 12. Appendices

### Appendix A: Glossary

| Term | Definition |
|------|------------|
| **3DGS** | 3D Gaussian Splatting - neural scene representation using anisotropic gaussians |
| **COLMAP** | Structure-from-Motion and Multi-View Stereo pipeline; de facto standard for camera pose estimation |
| **Spherical Harmonics (SH)** | Basis functions for representing view-dependent appearance in 3DGS |
| **Densification** | Process of adding new gaussians during 3DGS training to cover under-reconstructed regions |
| **Splatting** | Rendering technique projecting 3D primitives onto 2D image plane |
| **DXA** | Unit used in Word documents; 1440 DXA = 1 inch |
| **PLY** | Polygon File Format; commonly used for point cloud and mesh storage |

### Appendix B: Reference Implementations

| Repository | Purpose | License |
|------------|---------|---------|
| `graphdeco-inria/gaussian-splatting` | Canonical 3DGS implementation | Custom (research) |
| `nerfstudio-project/gsplat` | Optimized CUDA rasterizer | Apache 2.0 |
| `colmap/colmap` | SfM reference implementation | BSD |
| `antimatter15/splat` | Web-based PLY viewer | MIT |

### Appendix C: Hardware Requirements

**Development:**
- Windows 10/11 or Ubuntu 22.04+
- NVIDIA GPU with CUDA 11.8+ support
- 32GB+ RAM recommended
- 100GB+ SSD for build artifacts

**Runtime (Export Only):**
- Any system meeting UE5 requirements
- No GPU compute requirements

**Runtime (With Training):**
- NVIDIA GPU with 8GB+ VRAM
- CUDA 11.8+ drivers
- 64GB+ RAM for large scenes

### Appendix D: External Dependencies

| Dependency | Version | Purpose | License |
|------------|---------|---------|---------|
| Eigen | 3.4+ | Linear algebra | MPL2 |
| CUDA Toolkit | 11.8+ | GPU compute (optional) | NVIDIA EULA |
| rapidjson | 1.1+ | JSON parsing | MIT |
| stb_image_write | Latest | Image encoding | Public Domain |

---

## 13. Document Control

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-01 | Claude/DreamLab | Initial PRD |

---

