# Bounded Contexts

## Context Map Overview

The UE5-3DGS system is organised into four bounded contexts, each with distinct responsibilities and well-defined interfaces.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            CONTEXT MAP                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌────────────────────┐         ┌────────────────────┐                     │
│   │   SCENE CAPTURE    │ ──U/D──▶│  DATA EXTRACTION   │                     │
│   │     CONTEXT        │         │     CONTEXT        │                     │
│   └────────────────────┘         └────────────────────┘                     │
│            │                              │                                  │
│            │                              │                                  │
│            └──────────────┬───────────────┘                                  │
│                           │                                                  │
│                           ▼                                                  │
│                  ┌────────────────────┐                                      │
│                  │ FORMAT CONVERSION  │                                      │
│                  │     CONTEXT        │                                      │
│                  └────────────────────┘                                      │
│                           │                                                  │
│                           │ OHS                                              │
│                           ▼                                                  │
│                  ┌────────────────────┐                                      │
│                  │    TRAINING        │                                      │
│                  │    CONTEXT         │                                      │
│                  │   (Optional)       │                                      │
│                  └────────────────────┘                                      │
│                                                                              │
│   Legend: U/D = Upstream/Downstream, OHS = Open Host Service                 │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 1. Scene Capture Context

### Purpose
Orchestrates camera trajectory generation, frame rendering, and capture session management.

### Domain Language
- **Capture Session**: Active capture workflow instance
- **Trajectory**: Planned camera path through the scene
- **Coverage**: Percentage of scene geometry visible in frames
- **Frame**: Single rendered capture with metadata

### Aggregates
- `CaptureSession` (Root)
- `CapturedFrame`

### Key Responsibilities
1. Generate optimal camera trajectories for scene coverage
2. Execute high-fidelity rendering at configurable resolutions
3. Extract per-frame metadata with precise timestamps
4. Support batch and interactive capture modes

### C++ Module Interface

```cpp
// SCM/SCM_CaptureContext.h
#pragma once

#include "CoreMinimal.h"
#include "SCM_Types.h"

namespace SCM
{
    /**
     * Scene Capture Context - Entry point for capture operations
     */
    class FSCM_CaptureContext
    {
    public:
        // Session management
        TSharedPtr<FSCM_CaptureSession> CreateSession(
            UWorld* World,
            const FSCM_CaptureConfiguration& Config
        );

        TSharedPtr<FSCM_CaptureSession> GetActiveSession() const;
        void EndSession(const FGuid& SessionId);

        // Trajectory generation
        TSharedPtr<FSCM_Trajectory> GenerateTrajectory(
            ESCM_TrajectoryType Type,
            const FSCM_TrajectoryParams& Params
        );

        // Coverage analysis
        float AnalyseCoverage(
            const TArray<FTransform>& CameraPositions,
            const FBox& SceneBounds
        );

    private:
        TMap<FGuid, TSharedPtr<FSCM_CaptureSession>> ActiveSessions;
        TSharedPtr<FSCM_TrajectoryGenerator> TrajectoryGenerator;
        TSharedPtr<FSCM_CoverageAnalyser> CoverageAnalyser;
    };

    /**
     * Capture configuration value object
     */
    struct FSCM_CaptureConfiguration
    {
        FIntPoint Resolution = FIntPoint(1920, 1080);
        ESCM_TrajectoryType TrajectoryType = ESCM_TrajectoryType::Spherical;
        float TargetCoverage = 0.95f;
        float FieldOfView = 90.0f;
        float NearPlane = 10.0f;
        float FarPlane = 100000.0f;
        bool bCaptureDepth = true;
        bool bCaptureNormals = true;
        bool bCaptureMotionVectors = false;
    };
}
```

### Context Events Published
- `FCaptureStartedEvent`
- `FFrameRenderedEvent`
- `FCaptureCompletedEvent`
- `FCapturePausedEvent`
- `FCaptureResumedEvent`

### Dependencies
- Unreal Engine Renderer
- Movie Render Queue (optional)

---

## 2. Data Extraction Context

### Purpose
Extracts and transforms render buffers into standardised formats with correct coordinate system conversions.

### Domain Language
- **Buffer**: Render target containing colour, depth, or normal data
- **Intrinsics**: Internal camera calibration parameters
- **Extrinsics**: Camera pose in world coordinates
- **Convention**: Target coordinate system (UE5, COLMAP, OpenCV)

### Aggregates
- `SceneData` (Root)
- `CameraParameters`
- `RenderBuffer`

### Key Responsibilities
1. Extract RGB, depth, normals, and auxiliary buffers
2. Convert engine-space coordinates to standard conventions
3. Generate camera intrinsic/extrinsic parameters
4. Compute scene statistics and bounds

### C++ Module Interface

```cpp
// DEM/DEM_ExtractionContext.h
#pragma once

#include "CoreMinimal.h"
#include "DEM_Types.h"

namespace DEM
{
    /**
     * Data Extraction Context - Buffer extraction and transformation
     */
    class FDEM_ExtractionContext
    {
    public:
        // Scene data creation
        TSharedPtr<FDEM_SceneData> CreateSceneData(
            const SCM::FSCM_CaptureSession& Session
        );

        // Buffer extraction
        TSharedPtr<FDEM_ColorBuffer> ExtractColorBuffer(
            UTextureRenderTarget2D* RenderTarget,
            EDEM_ColorSpace ColorSpace = EDEM_ColorSpace::sRGB
        );

        TSharedPtr<FDEM_DepthBuffer> ExtractDepthBuffer(
            UTextureRenderTarget2D* RenderTarget,
            float NearPlane,
            float FarPlane
        );

        TSharedPtr<FDEM_NormalBuffer> ExtractNormalBuffer(
            UTextureRenderTarget2D* RenderTarget,
            EDEM_NormalSpace Space = EDEM_NormalSpace::WorldSpace
        );

        // Camera parameter generation
        FDEM_CameraParameters ComputeCameraParameters(
            const FTransform& CameraTransform,
            const FMinimalViewInfo& ViewInfo,
            EDEM_CoordinateConvention TargetConvention
        );

        // Coordinate conversion
        FMatrix ConvertTransformToConvention(
            const FTransform& Transform,
            EDEM_CoordinateConvention From,
            EDEM_CoordinateConvention To
        );

    private:
        TSharedPtr<FDEM_BufferExtractor> BufferExtractor;
        TSharedPtr<FDEM_CoordinateConverter> CoordinateConverter;
    };

    /**
     * Coordinate convention enumeration
     */
    enum class EDEM_CoordinateConvention : uint8
    {
        UE5_LeftHanded_ZUp,    // Native UE5: X-forward, Y-right, Z-up
        OpenCV_RightHanded,     // OpenCV: X-right, Y-down, Z-forward
        OpenGL_RightHanded,     // OpenGL: X-right, Y-up, Z-backward
        COLMAP_RightHanded,     // COLMAP: X-right, Y-down, Z-forward
        Blender_RightHanded_ZUp // Blender: X-right, Y-forward, Z-up
    };
}
```

### Context Events Published
- `FBufferExtractedEvent`
- `FCameraParametersComputedEvent`
- `FSceneDataReadyEvent`

### Upstream Dependencies
- Scene Capture Context (provides CaptureSession data)

---

## 3. Format Conversion Context

### Purpose
Converts extracted scene data into industry-standard export formats for 3DGS training pipelines.

### Domain Language
- **COLMAP**: Standard camera pose format (text/binary)
- **PLY**: Point cloud format for gaussian splats
- **Manifest**: JSON metadata describing the export
- **Export Job**: Async conversion operation

### Aggregates
- `ExportJob` (Root)
- `ExportConfiguration`
- `ExportResult`

### Key Responsibilities
1. Export to COLMAP database and text formats
2. Generate 3DGS-compatible PLY files
3. Create training configuration manifests
4. Support custom binary formats for streaming

### C++ Module Interface

```cpp
// FCM/FCM_ConversionContext.h
#pragma once

#include "CoreMinimal.h"
#include "FCM_Types.h"

namespace FCM
{
    /**
     * Format Conversion Context - Export format management
     */
    class FFCM_ConversionContext
    {
    public:
        // Export job management
        TSharedPtr<FFCM_ExportJob> CreateExportJob(
            const FFCM_ExportConfiguration& Config
        );

        TFuture<FFCM_ExportResult> ExecuteExportAsync(
            TSharedPtr<FFCM_ExportJob> Job,
            const DEM::FDEM_SceneData& SceneData
        );

        void CancelExport(const FGuid& JobId);

        // Format-specific writers
        TSharedPtr<FFCM_COLMAPWriter> GetCOLMAPWriter();
        TSharedPtr<FFCM_PLYWriter> GetPLYWriter();
        TSharedPtr<FFCM_ManifestWriter> GetManifestWriter();

        // Validation
        bool ValidateExport(
            const FString& OutputPath,
            EFFCM_ExportFormat Format
        );

    private:
        TSharedPtr<FFCM_COLMAPWriter> COLMAPWriter;
        TSharedPtr<FFCM_PLYWriter> PLYWriter;
        TSharedPtr<FFCM_ManifestWriter> ManifestWriter;
        TMap<FGuid, TSharedPtr<FFCM_ExportJob>> ActiveJobs;
    };

    /**
     * Export configuration
     */
    struct FFCM_ExportConfiguration
    {
        FString OutputPath;
        EFFCM_ExportFormat Format = EFFCM_ExportFormat::COLMAP_Binary;
        bool bExportImages = true;
        bool bExportDepthMaps = true;
        bool bExportNormalMaps = false;
        bool bGenerateManifest = true;
        FString ImageFormat = TEXT("png");
        int32 ImageQuality = 95;
    };

    /**
     * Export format enumeration
     */
    enum class EFFCM_ExportFormat : uint8
    {
        COLMAP_Text,
        COLMAP_Binary,
        PLY_ASCII,
        PLY_Binary,
        Custom_Streaming
    };
}
```

### Context Events Published
- `FExportStartedEvent`
- `FExportProgressEvent`
- `FExportCompletedEvent`
- `FExportFailedEvent`

### Upstream Dependencies
- Data Extraction Context (provides SceneData)

---

## 4. Training Context (Optional)

### Purpose
Provides optional in-engine 3DGS training capabilities using CUDA acceleration.

### Domain Language
- **Gaussian**: Individual splat primitive with SH coefficients
- **Densification**: Adding new gaussians to under-reconstructed regions
- **Pruning**: Removing low-opacity or oversized gaussians
- **Iteration**: Single optimisation step

### Aggregates
- `TrainingConfig` (Root)
- `TrainingSession`
- `GaussianCloud`

### Key Responsibilities
1. Initialise CUDA context and GPU memory
2. Execute iterative gaussian optimisation
3. Perform adaptive densification and pruning
4. Export trained gaussian splats to PLY

### C++ Module Interface

```cpp
// TRN/TRN_TrainingContext.h
#pragma once

#include "CoreMinimal.h"
#include "TRN_Types.h"

namespace TRN
{
    /**
     * Training Context - In-engine 3DGS training (Optional)
     */
    class FTRN_TrainingContext
    {
    public:
        // Initialisation
        bool InitialiseCUDA();
        bool IsCUDAAvailable() const;
        void ShutdownCUDA();

        // Training session
        TSharedPtr<FTRN_TrainingSession> CreateSession(
            const FTRN_TrainingConfig& Config,
            const FCM::FFCM_ExportResult& TrainingData
        );

        void StartTraining(TSharedPtr<FTRN_TrainingSession> Session);
        void PauseTraining(TSharedPtr<FTRN_TrainingSession> Session);
        void StopTraining(TSharedPtr<FTRN_TrainingSession> Session);

        // Results
        TSharedPtr<FTRN_GaussianCloud> GetTrainedModel(
            TSharedPtr<FTRN_TrainingSession> Session
        );

        void ExportModel(
            TSharedPtr<FTRN_GaussianCloud> Model,
            const FString& OutputPath,
            EFFCM_ExportFormat Format = EFFCM_ExportFormat::PLY_Binary
        );

    private:
        void* CudaContext = nullptr;
        void* CudaStream = nullptr;
        TMap<FGuid, TSharedPtr<FTRN_TrainingSession>> ActiveSessions;
    };

    /**
     * Training configuration
     */
    struct FTRN_TrainingConfig
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
        float ScalePruneThreshold = 0.1f;

        // Loss
        float LambdaL1 = 0.8f;
        float LambdaSSIM = 0.2f;
    };
}
```

### Context Events Published
- `FTrainingStartedEvent`
- `FIterationCompletedEvent`
- `FDensificationEvent`
- `FPruningEvent`
- `FTrainingCompletedEvent`

### Upstream Dependencies
- Format Conversion Context (provides COLMAP training data)

---

## Context Integration Patterns

### Shared Kernel

Common types shared across all contexts:

```cpp
// Shared/Shared_Kernel.h
#pragma once

#include "CoreMinimal.h"

namespace Shared
{
    // Common result type
    template<typename T>
    struct TResult
    {
        bool bSuccess;
        T Value;
        FString ErrorMessage;

        static TResult Success(T InValue) { return {true, InValue, TEXT("")}; }
        static TResult Failure(const FString& Error) { return {false, T{}, Error}; }
    };

    // Common event base
    struct FDomainEvent
    {
        FGuid EventId;
        FDateTime Timestamp;
        FString ContextName;
    };
}
```

### Anti-Corruption Layer

Prevents UE5 concepts from leaking into domain:

```cpp
// ACL/ACL_UnrealAdapter.h
class FACL_UnrealAdapter
{
public:
    // Convert UE5 types to domain types
    static FSCM_CapturedFrame FromSceneCapture2D(
        USceneCaptureComponent2D* Component
    );

    static FDEM_CameraParameters FromCineCamera(
        ACineCameraActor* Camera
    );

    // Convert domain types to UE5 types
    static void ApplyToCineCamera(
        const FDEM_CameraParameters& Params,
        ACineCameraActor* Camera
    );
};
```
