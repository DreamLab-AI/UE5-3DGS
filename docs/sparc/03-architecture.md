# SPARC Architecture Document
## UE5-3DGS: System Architecture

**Version:** 1.0
**Phase:** Architecture
**Status:** Draft

---

## 1. System Overview

### 1.1 Component Diagram

```mermaid
graph TB
    subgraph "UE5-3DGS Plugin"
        subgraph "Scene Capture Module (SCM)"
            SCM_Orch[CaptureOrchestrator]
            SCM_Traj[TrajectoryGenerator]
            SCM_Frame[FrameRenderer]
            SCM_Cover[CoverageAnalyser]
        end

        subgraph "Data Extraction Module (DEM)"
            DEM_Color[ColorExtractor]
            DEM_Depth[DepthExtractor]
            DEM_Normal[NormalExtractor]
            DEM_Coord[CoordinateConverter]
            DEM_Params[CameraParameters]
        end

        subgraph "Format Conversion Module (FCM)"
            FCM_COLMAP[COLMAPWriter]
            FCM_PLY[PLYWriter]
            FCM_Manifest[ManifestGenerator]
        end

        subgraph "Training Module (TRN) - Optional"
            TRN_CUDA[CUDAIntegration]
            TRN_Opt[Optimizer]
            TRN_Rast[Rasterizer]
        end

        subgraph "Editor Integration"
            UI_Mode[EditorMode]
            UI_Widget[CaptureWidget]
            UI_Visual[Visualizer]
        end
    end

    subgraph "External Systems"
        UE5_World[(UE5 World)]
        UE5_RHI[RHI/Renderer]
        FileSystem[(File System)]
        CUDA_Runtime[CUDA Runtime]
    end

    %% Connections
    SCM_Orch --> SCM_Traj
    SCM_Orch --> SCM_Frame
    SCM_Orch --> SCM_Cover

    SCM_Frame --> DEM_Color
    SCM_Frame --> DEM_Depth
    SCM_Frame --> DEM_Normal

    DEM_Color --> FCM_COLMAP
    DEM_Depth --> FCM_COLMAP
    DEM_Coord --> FCM_COLMAP
    DEM_Params --> FCM_COLMAP

    FCM_COLMAP --> FCM_Manifest
    FCM_COLMAP --> TRN_CUDA

    UE5_World --> SCM_Orch
    UE5_RHI --> SCM_Frame
    FCM_COLMAP --> FileSystem
    TRN_CUDA --> CUDA_Runtime

    UI_Mode --> SCM_Orch
    UI_Widget --> SCM_Orch
    UI_Visual --> SCM_Cover
```

### 1.2 Module Responsibilities

| Module | Responsibility | Dependencies |
|--------|---------------|--------------|
| SCM | Camera trajectory and capture orchestration | UWorld, RenderCore |
| DEM | Buffer extraction and coordinate conversion | RHI, RenderCore |
| FCM | File format writing | ImageWrapper, FileManager |
| TRN | Optional in-engine training | CUDA, FCM output |
| UI | Editor tools and visualization | Slate, EditorFramework |

---

## 2. Data Flow Architecture

### 2.1 Capture Pipeline Data Flow

```mermaid
flowchart LR
    subgraph Input
        World[UWorld Scene]
        Config[Capture Config]
    end

    subgraph SCM["Scene Capture Module"]
        TrajGen[Generate Trajectory]
        FrameCap[Capture Frame]
        CoverCheck[Check Coverage]
    end

    subgraph DEM["Data Extraction Module"]
        ExtractRGB[Extract RGB]
        ExtractDepth[Extract Depth]
        ExtractNormal[Extract Normal]
        ConvertCoords[Convert Coords]
    end

    subgraph FCM["Format Conversion Module"]
        WriteImages[Write Images]
        WriteCameras[Write Cameras]
        WriteManifest[Write Manifest]
    end

    subgraph Output
        COLMAP[(COLMAP Dataset)]
        PLY[(PLY File)]
        Manifest[(Manifest.json)]
    end

    World --> TrajGen
    Config --> TrajGen
    TrajGen --> FrameCap
    FrameCap --> CoverCheck
    CoverCheck -->|More frames needed| FrameCap

    FrameCap --> ExtractRGB
    FrameCap --> ExtractDepth
    FrameCap --> ExtractNormal
    ExtractRGB --> ConvertCoords
    ExtractDepth --> ConvertCoords

    ConvertCoords --> WriteImages
    ConvertCoords --> WriteCameras
    WriteImages --> COLMAP
    WriteCameras --> COLMAP
    WriteImages --> WriteManifest
    WriteManifest --> Manifest
```

### 2.2 Data Structures Flow

```mermaid
classDiagram
    class FCaptureConfig {
        +FIntPoint Resolution
        +ESCM_TrajectoryType Trajectory
        +int32 NumFrames
        +float SphereRadius
        +FString OutputPath
    }

    class FCapturedFrame {
        +int32 FrameIndex
        +double Timestamp
        +FTransform CameraTransform
        +TSharedPtr~FImage~ ColorBuffer
        +TArray~float~ DepthBuffer
        +FCameraIntrinsics Intrinsics
    }

    class FCameraIntrinsics {
        +float FocalX
        +float FocalY
        +float PrincipalX
        +float PrincipalY
        +int32 Width
        +int32 Height
    }

    class FCOLMAPCamera {
        +int32 CameraID
        +ECameraModel Model
        +int32 Width
        +int32 Height
        +TArray~double~ Params
    }

    class FCOLMAPImage {
        +int32 ImageID
        +FQuat Quaternion
        +FVector Translation
        +int32 CameraID
        +FString Name
    }

    FCaptureConfig --> FCapturedFrame : produces
    FCapturedFrame --> FCameraIntrinsics : contains
    FCapturedFrame --> FCOLMAPCamera : converts to
    FCapturedFrame --> FCOLMAPImage : converts to
```

---

## 3. Sequence Diagrams

### 3.1 Capture Workflow Sequence

```mermaid
sequenceDiagram
    participant User
    participant EditorMode
    participant Orchestrator
    participant TrajectoryGen
    participant FrameRenderer
    participant BufferExtractor
    participant COLMAPWriter

    User->>EditorMode: Start Capture
    EditorMode->>Orchestrator: ExecuteCaptureAsync(World, Config)

    Orchestrator->>TrajectoryGen: GenerateTrajectory(Config)
    TrajectoryGen-->>Orchestrator: TArray<FTransform>

    loop For Each Camera Position
        Orchestrator->>FrameRenderer: RenderFrame(Transform)
        FrameRenderer->>FrameRenderer: SetupSceneCapture
        FrameRenderer->>FrameRenderer: CaptureScene
        FrameRenderer-->>Orchestrator: RenderTargets

        Orchestrator->>BufferExtractor: ExtractBuffers(RenderTargets)
        BufferExtractor->>BufferExtractor: ExtractColor
        BufferExtractor->>BufferExtractor: ExtractDepth
        BufferExtractor->>BufferExtractor: ConvertCoordinates
        BufferExtractor-->>Orchestrator: FCapturedFrame

        Orchestrator->>EditorMode: OnProgress(Current, Total)
    end

    Orchestrator->>COLMAPWriter: WriteDataset(Frames)
    COLMAPWriter->>COLMAPWriter: WriteCameras
    COLMAPWriter->>COLMAPWriter: WriteImages
    COLMAPWriter->>COLMAPWriter: WriteManifest
    COLMAPWriter-->>Orchestrator: Success

    Orchestrator-->>EditorMode: CaptureComplete
    EditorMode-->>User: Show Results
```

### 3.2 Export Workflow Sequence

```mermaid
sequenceDiagram
    participant Orchestrator
    participant CoordConverter
    participant COLMAPWriter
    participant ImageWriter
    participant FileSystem

    Orchestrator->>COLMAPWriter: WriteDataset(Frames, OutputPath, bBinary)

    COLMAPWriter->>FileSystem: CreateDirectory(OutputPath/images)
    COLMAPWriter->>FileSystem: CreateDirectory(OutputPath/sparse/0)

    par Write Images
        loop For Each Frame
            COLMAPWriter->>ImageWriter: WriteImage(Frame.ColorBuffer, Path)
            ImageWriter->>FileSystem: Write PNG/JPG
        end
    and Write Camera Data
        COLMAPWriter->>CoordConverter: ConvertAllToCOLMAP(Frames)
        CoordConverter-->>COLMAPWriter: COLMAPData

        alt Binary Format
            COLMAPWriter->>FileSystem: Write cameras.bin
            COLMAPWriter->>FileSystem: Write images.bin
            COLMAPWriter->>FileSystem: Write points3D.bin
        else Text Format
            COLMAPWriter->>FileSystem: Write cameras.txt
            COLMAPWriter->>FileSystem: Write images.txt
            COLMAPWriter->>FileSystem: Write points3D.txt
        end
    end

    COLMAPWriter->>FileSystem: Write manifest.json
    COLMAPWriter-->>Orchestrator: ExportResult
```

### 3.3 Training Workflow Sequence (Phase 2)

```mermaid
sequenceDiagram
    participant User
    participant TrainingModule
    participant CUDAContext
    participant DataLoader
    participant Optimizer
    participant Rasterizer
    participant Densifier

    User->>TrainingModule: StartTraining(COLMAPPath, Config)

    TrainingModule->>CUDAContext: Initialize()
    CUDAContext-->>TrainingModule: CUDA Ready

    TrainingModule->>DataLoader: LoadDataset(COLMAPPath)
    DataLoader-->>TrainingModule: Images, Cameras

    TrainingModule->>TrainingModule: InitializeGaussians(SparsePoints)

    loop Training Iterations
        TrainingModule->>Rasterizer: RenderView(RandomCamera)
        Rasterizer-->>TrainingModule: RenderedImage

        TrainingModule->>TrainingModule: ComputeLoss(Rendered, GroundTruth)
        TrainingModule->>Optimizer: Backward(Loss)
        Optimizer->>Optimizer: UpdateParameters()

        alt Densification Interval
            TrainingModule->>Densifier: EvaluateDensification()
            Densifier->>Densifier: Clone/Split High Gradient
            Densifier->>Densifier: Prune Low Opacity
        end

        TrainingModule->>User: OnIterationComplete(Metrics)
    end

    TrainingModule->>TrainingModule: ExportPLY()
    TrainingModule-->>User: TrainingComplete
```

---

## 4. Class Architecture

### 4.1 Core Classes UML

```mermaid
classDiagram
    class IUE5_3DGS_Module {
        <<interface>>
        +StartupModule()
        +ShutdownModule()
    }

    class FUE5_3DGS_Module {
        -TSharedPtr~Orchestrator~ MainOrchestrator
        +StartupModule()
        +ShutdownModule()
        +GetOrchestrator() Orchestrator
    }

    class FSCM_CaptureOrchestrator {
        -FSCM_TrajectoryGenerator* TrajectoryGen
        -FSCM_FrameRenderer* FrameRenderer
        -FSCM_CoverageAnalyser* CoverageAnalyser
        -FCaptureConfig CurrentConfig
        -TArray~FCapturedFrame~ CapturedFrames
        +ExecuteCaptureAsync(World, Config) TFuture
        +PauseCapture()
        +ResumeCapture()
        +CancelCapture()
        +GetProgress() float
    }

    class FSCM_TrajectoryGenerator {
        <<abstract>>
        +Generate(Config) TArray~FTransform~
        +GetTrajectoryType() ESCM_TrajectoryType
    }

    class FSCM_SphericalTrajectory {
        +Generate(Config) TArray~FTransform~
        -GenerateFibonacciLattice(N, Radius)
    }

    class FSCM_OrbitalTrajectory {
        +Generate(Config) TArray~FTransform~
        -GenerateOrbitPath(Radius, Elevation, Steps)
    }

    class FSCM_FrameRenderer {
        -USceneCaptureComponent2D* CaptureComponent
        -UTextureRenderTarget2D* ColorTarget
        -UTextureRenderTarget2D* DepthTarget
        +RenderFrame(Transform) FRenderTargets
        +SetResolution(Resolution)
    }

    class FDEM_CoordinateConverter {
        +ConvertToCOLMAP(UE5Transform) FCOLMAPExtrinsics
        +ConvertFromCOLMAP(COLMAPExtrinsics) FTransform
        +ConvertPosition(Position, Convention) FVector
        +ConvertRotation(Rotation, Convention) FQuat
    }

    class FFCM_COLMAPWriter {
        +WriteDataset(Path, Frames, bBinary) bool
        -WriteCamerasText(Path, Cameras) bool
        -WriteCamerasBinary(Path, Cameras) bool
        -WriteImagesText(Path, Images) bool
        -WriteImagesBinary(Path, Images) bool
        -WritePoints3DText(Path) bool
        -WritePoints3DBinary(Path) bool
    }

    IUE5_3DGS_Module <|.. FUE5_3DGS_Module
    FUE5_3DGS_Module *-- FSCM_CaptureOrchestrator
    FSCM_CaptureOrchestrator *-- FSCM_TrajectoryGenerator
    FSCM_CaptureOrchestrator *-- FSCM_FrameRenderer
    FSCM_TrajectoryGenerator <|-- FSCM_SphericalTrajectory
    FSCM_TrajectoryGenerator <|-- FSCM_OrbitalTrajectory
    FSCM_CaptureOrchestrator --> FDEM_CoordinateConverter
    FSCM_CaptureOrchestrator --> FFCM_COLMAPWriter
```

### 4.2 Buffer Extraction Classes

```mermaid
classDiagram
    class IDEM_BufferExtractor {
        <<interface>>
        +ExtractBuffer(RenderTarget) TSharedPtr~FBuffer~
        +GetBufferType() EBufferType
    }

    class FDEM_ColorExtractor {
        -EDEM_ColorSpace TargetColorSpace
        +ExtractBuffer(RenderTarget) TSharedPtr~FImage~
        +SetColorSpace(ColorSpace)
        -ConvertToSRGB(LinearData) void
    }

    class FDEM_DepthExtractor {
        -float NearPlane
        -float FarPlane
        -bool bInfiniteFar
        +ExtractBuffer(RenderTarget) TSharedPtr~FFloatBuffer~
        +SetClipPlanes(Near, Far)
        -ConvertReverseZToLinear(RawDepth) float
    }

    class FDEM_NormalExtractor {
        -EDEM_NormalSpace TargetSpace
        +ExtractBuffer(RenderTarget) TSharedPtr~FVectorBuffer~
        +SetNormalSpace(Space)
        -ConvertToWorldSpace(ViewNormal, ViewMatrix) FVector
    }

    IDEM_BufferExtractor <|.. FDEM_ColorExtractor
    IDEM_BufferExtractor <|.. FDEM_DepthExtractor
    IDEM_BufferExtractor <|.. FDEM_NormalExtractor
```

---

## 5. Plugin Architecture

### 5.1 Directory Structure

```
UE5_3DGS/
├── Source/
│   ├── UE5_3DGS/                          # Runtime module
│   │   ├── Public/
│   │   │   ├── UE5_3DGS.h                 # Module header
│   │   │   ├── SCM/
│   │   │   │   ├── SCM_CaptureOrchestrator.h
│   │   │   │   ├── SCM_TrajectoryGenerator.h
│   │   │   │   ├── SCM_SphericalTrajectory.h
│   │   │   │   ├── SCM_OrbitalTrajectory.h
│   │   │   │   ├── SCM_SplineTrajectory.h
│   │   │   │   ├── SCM_FrameRenderer.h
│   │   │   │   └── SCM_CoverageAnalyser.h
│   │   │   ├── DEM/
│   │   │   │   ├── DEM_BufferExtractor.h
│   │   │   │   ├── DEM_ColorExtractor.h
│   │   │   │   ├── DEM_DepthExtractor.h
│   │   │   │   ├── DEM_NormalExtractor.h
│   │   │   │   ├── DEM_CoordinateConverter.h
│   │   │   │   └── DEM_CameraParameters.h
│   │   │   ├── FCM/
│   │   │   │   ├── FCM_COLMAPWriter.h
│   │   │   │   ├── FCM_PLYWriter.h
│   │   │   │   └── FCM_ManifestGenerator.h
│   │   │   └── Types/
│   │   │       ├── UE5_3DGS_Types.h
│   │   │       └── UE5_3DGS_Enums.h
│   │   ├── Private/
│   │   │   ├── UE5_3DGS.cpp
│   │   │   ├── SCM/
│   │   │   │   └── [Implementation files]
│   │   │   ├── DEM/
│   │   │   │   └── [Implementation files]
│   │   │   └── FCM/
│   │   │       └── [Implementation files]
│   │   └── UE5_3DGS.Build.cs
│   │
│   ├── UE5_3DGS_Editor/                   # Editor module
│   │   ├── Public/
│   │   │   ├── UE5_3DGS_EditorMode.h
│   │   │   ├── UE5_3DGS_EditorCommands.h
│   │   │   └── Widgets/
│   │   │       ├── SUE5_3DGS_CapturePanel.h
│   │   │       └── SUE5_3DGS_ProgressBar.h
│   │   ├── Private/
│   │   │   └── [Implementation files]
│   │   └── UE5_3DGS_Editor.Build.cs
│   │
│   └── UE5_3DGS_Training/                 # Optional training module
│       ├── Public/
│       │   ├── TRN_CUDAContext.h
│       │   ├── TRN_GaussianOptimizer.h
│       │   └── TRN_TileRasterizer.h
│       ├── Private/
│       │   └── [Implementation files]
│       └── UE5_3DGS_Training.Build.cs
│
├── Shaders/
│   └── Private/
│       ├── DepthExtraction.usf
│       ├── NormalExtraction.usf
│       └── CoverageVisualization.usf
│
├── Content/
│   ├── Blueprints/
│   │   └── BP_3DGS_CaptureActor.uasset
│   ├── Materials/
│   │   ├── M_DepthVisualization.uasset
│   │   └── M_CoverageHeatmap.uasset
│   └── UI/
│       └── WBP_CapturePanel.uasset
│
├── ThirdParty/
│   ├── Eigen/
│   │   └── [Eigen 3.4+ headers]
│   └── CUDA/
│       └── [CUDA headers - optional]
│
├── Config/
│   └── DefaultUE5_3DGS.ini
│
├── Resources/
│   └── Icon128.png
│
└── UE5_3DGS.uplugin
```

### 5.2 Module Dependencies

```mermaid
graph TD
    subgraph "UE5 Engine Modules"
        Core[Core]
        CoreUObject[CoreUObject]
        Engine[Engine]
        RenderCore[RenderCore]
        RHI[RHI]
        Renderer[Renderer]
        Slate[Slate]
        SlateCore[SlateCore]
        EditorFramework[EditorFramework]
        UnrealEd[UnrealEd]
    end

    subgraph "Plugin Modules"
        UE5_3DGS[UE5_3DGS Runtime]
        UE5_3DGS_Editor[UE5_3DGS_Editor]
        UE5_3DGS_Training[UE5_3DGS_Training]
    end

    subgraph "Third Party"
        Eigen[Eigen]
        CUDA[CUDA Toolkit]
    end

    UE5_3DGS --> Core
    UE5_3DGS --> CoreUObject
    UE5_3DGS --> Engine
    UE5_3DGS --> RenderCore
    UE5_3DGS --> RHI
    UE5_3DGS --> Renderer
    UE5_3DGS --> Eigen

    UE5_3DGS_Editor --> UE5_3DGS
    UE5_3DGS_Editor --> Slate
    UE5_3DGS_Editor --> SlateCore
    UE5_3DGS_Editor --> EditorFramework
    UE5_3DGS_Editor --> UnrealEd

    UE5_3DGS_Training --> UE5_3DGS
    UE5_3DGS_Training --> CUDA
```

---

## 6. Interface Design

### 6.1 Public C++ API

```cpp
// UE5_3DGS.h - Main public interface

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Forward declarations
class FSCM_CaptureOrchestrator;
struct FCaptureConfig;
struct FCaptureResult;

/**
 * Main module interface for UE5-3DGS plugin
 */
class IUE5_3DGS_Module : public IModuleInterface
{
public:
    /**
     * Get module singleton
     */
    static IUE5_3DGS_Module& Get()
    {
        return FModuleManager::LoadModuleChecked<IUE5_3DGS_Module>("UE5_3DGS");
    }

    /**
     * Check if module is loaded
     */
    static bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("UE5_3DGS");
    }

    /**
     * Get the capture orchestrator for programmatic control
     */
    virtual TSharedPtr<FSCM_CaptureOrchestrator> GetOrchestrator() = 0;

    /**
     * Quick capture with default settings
     * @param World Target world to capture
     * @param OutputPath Output directory path
     * @param NumFrames Number of frames to capture
     * @return Async future with capture result
     */
    virtual TFuture<FCaptureResult> QuickCapture(
        UWorld* World,
        const FString& OutputPath,
        int32 NumFrames = 100
    ) = 0;
};

/**
 * Capture configuration structure
 */
USTRUCT(BlueprintType)
struct FCaptureConfig
{
    GENERATED_BODY()

    /** Output resolution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Resolution = FIntPoint(1920, 1080);

    /** Trajectory type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESCM_TrajectoryType TrajectoryType = ESCM_TrajectoryType::Spherical;

    /** Number of frames to capture */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="10000"))
    int32 NumFrames = 100;

    /** Sphere radius for spherical/orbital trajectory (auto if <= 0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SphereRadius = 0.0f;

    /** Output directory path */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OutputPath;

    /** Export format */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFCM_ExportFormat ExportFormat = EFCM_ExportFormat::COLMAP_Binary;

    /** Include depth maps in export */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExportDepth = false;

    /** Include normal maps in export */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExportNormals = false;
};
```

### 6.2 Blueprint Interface

```cpp
// UE5_3DGS_BlueprintLibrary.h

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "UE5_3DGS_BlueprintLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnCaptureProgress, int32, CurrentFrame, int32, TotalFrames);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnCaptureComplete, bool, bSuccess);

/**
 * Blueprint function library for UE5-3DGS
 */
UCLASS()
class UE5_3DGS_API UUE5_3DGS_BlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Start asynchronous capture session
     * @param WorldContextObject World context
     * @param Config Capture configuration
     * @param OnProgress Progress callback
     * @param OnComplete Completion callback
     */
    UFUNCTION(BlueprintCallable, Category = "3DGS", meta = (WorldContext = "WorldContextObject"))
    static void StartCapture(
        UObject* WorldContextObject,
        const FCaptureConfig& Config,
        FOnCaptureProgress OnProgress,
        FOnCaptureComplete OnComplete
    );

    /**
     * Cancel active capture session
     */
    UFUNCTION(BlueprintCallable, Category = "3DGS")
    static void CancelCapture();

    /**
     * Check if capture is in progress
     */
    UFUNCTION(BlueprintPure, Category = "3DGS")
    static bool IsCaptureInProgress();

    /**
     * Get current capture progress (0.0 - 1.0)
     */
    UFUNCTION(BlueprintPure, Category = "3DGS")
    static float GetCaptureProgress();

    /**
     * Generate trajectory preview transforms
     * @param WorldContextObject World context
     * @param Config Capture configuration
     * @return Array of camera transforms for preview
     */
    UFUNCTION(BlueprintCallable, Category = "3DGS", meta = (WorldContext = "WorldContextObject"))
    static TArray<FTransform> GenerateTrajectoryPreview(
        UObject* WorldContextObject,
        const FCaptureConfig& Config
    );
};
```

---

## 7. Error Handling Architecture

### 7.1 Error Propagation

```mermaid
flowchart TD
    subgraph "Error Sources"
        FileIO[File I/O Error]
        GPU[GPU/Render Error]
        Memory[Memory Error]
        Config[Config Validation]
    end

    subgraph "Error Handling Layer"
        TryOp[Try Operation]
        CatchErr[Catch Exception/Error]
        LogErr[Log Error]
        NotifyUI[Notify UI]
    end

    subgraph "Recovery Actions"
        Retry[Retry Operation]
        Fallback[Use Fallback]
        Abort[Abort with Cleanup]
        Report[Report to User]
    end

    FileIO --> TryOp
    GPU --> TryOp
    Memory --> TryOp
    Config --> TryOp

    TryOp --> CatchErr
    CatchErr --> LogErr
    LogErr --> NotifyUI

    NotifyUI --> Retry
    NotifyUI --> Fallback
    NotifyUI --> Abort
    Abort --> Report
```

### 7.2 Error Codes Enum

```cpp
UENUM(BlueprintType)
enum class EUE5_3DGS_ErrorCode : uint8
{
    Success = 0,

    // Configuration Errors (100-199)
    E_InvalidWorld = 100,
    E_InvalidConfig = 101,
    E_PathNotWritable = 102,
    E_InsufficientDiskSpace = 103,

    // Capture Errors (200-299)
    E_TrajectoryGenerationFailed = 200,
    E_RenderTargetCreationFailed = 201,
    E_FrameCaptureFailed = 202,
    E_BufferReadbackFailed = 203,
    E_CaptureTimeout = 204,

    // Export Errors (300-399)
    E_FileWriteFailed = 300,
    E_ImageEncodingFailed = 301,
    E_CoordinateConversionFailed = 302,
    E_InvalidExportFormat = 303,

    // Training Errors (400-499)
    E_CUDAInitFailed = 400,
    E_CUDAOutOfMemory = 401,
    E_TrainingDiverged = 402,
    E_InvalidTrainingData = 403,

    // Internal Errors (500-599)
    E_InternalError = 500,
    E_NotImplemented = 501
};
```

---

## 8. Configuration Architecture

### 8.1 Config File Schema

```ini
; DefaultUE5_3DGS.ini

[/Script/UE5_3DGS.UE5_3DGS_Settings]

; Default capture settings
DefaultResolution=(X=1920,Y=1080)
DefaultNumFrames=100
DefaultTrajectoryType=Spherical
DefaultExportFormat=COLMAP_Binary

; Performance settings
MaxConcurrentFrames=4
AsyncIOEnabled=true
GPUReadbackPoolSize=8

; Quality settings
DefaultImageFormat=PNG
JPEGQuality=95
DepthPrecision=32

; Coordinate conversion
DefaultCoordinateConvention=COLMAP_RightHanded
DefaultUnitScale=0.01

; Paths
DefaultOutputPath=Saved/3DGS_Export
TempBufferPath=Saved/3DGS_Temp

; Training (optional module)
CUDADeviceIndex=0
TrainingIterations=30000
CheckpointInterval=5000
```

### 8.2 Settings Class

```cpp
UCLASS(config=UE5_3DGS, defaultconfig)
class UE5_3DGS_API UUE5_3DGS_Settings : public UObject
{
    GENERATED_BODY()

public:
    UUE5_3DGS_Settings();

    /** Default capture resolution */
    UPROPERTY(config, EditAnywhere, Category = "Capture")
    FIntPoint DefaultResolution;

    /** Default number of frames */
    UPROPERTY(config, EditAnywhere, Category = "Capture")
    int32 DefaultNumFrames;

    /** Default trajectory type */
    UPROPERTY(config, EditAnywhere, Category = "Capture")
    ESCM_TrajectoryType DefaultTrajectoryType;

    /** Maximum concurrent frame captures */
    UPROPERTY(config, EditAnywhere, Category = "Performance")
    int32 MaxConcurrentFrames;

    /** Enable async I/O for exports */
    UPROPERTY(config, EditAnywhere, Category = "Performance")
    bool bAsyncIOEnabled;

    /** Get singleton settings object */
    static UUE5_3DGS_Settings* Get();
};
```

---

## 9. Memory Management

### 9.1 Buffer Pool Architecture

```mermaid
graph TD
    subgraph "Buffer Pool Manager"
        PoolMgr[FBufferPoolManager]
        ColorPool[Color Buffer Pool]
        DepthPool[Depth Buffer Pool]
        NormalPool[Normal Buffer Pool]
    end

    subgraph "Frame Capture Pipeline"
        Capture[Frame Capture]
        Extract[Buffer Extract]
        Convert[Convert/Write]
        Release[Release Buffer]
    end

    PoolMgr --> ColorPool
    PoolMgr --> DepthPool
    PoolMgr --> NormalPool

    Capture -->|Acquire| ColorPool
    Capture -->|Acquire| DepthPool
    Extract -->|Use| ColorPool
    Extract -->|Use| DepthPool
    Convert -->|Read| ColorPool
    Convert -->|Read| DepthPool
    Release -->|Return| ColorPool
    Release -->|Return| DepthPool
```

### 9.2 Memory Budget

| Resource | Budget | Notes |
|----------|--------|-------|
| Render Target Pool | 4 GB | 8x 4K RGBA + Depth |
| Frame Queue | 10 frames | Async pipeline depth |
| Export Buffer | 1 GB | Streaming write |
| Training (GPU) | 8 GB | Gaussian parameters + optimizer state |

---

## 10. Platform Considerations

### 10.1 Platform Support Matrix

| Platform | Capture | Export | Training |
|----------|---------|--------|----------|
| Windows (DX12) | Full | Full | Full (CUDA) |
| Windows (DX11) | Full | Full | Limited |
| Linux (Vulkan) | Full | Full | Full (CUDA) |
| macOS (Metal) | Full | Full | Not Supported |

### 10.2 API Abstraction

```cpp
// Platform-specific render target handling
class FDEM_PlatformBufferExtractor
{
public:
    static TSharedPtr<FDEM_PlatformBufferExtractor> Create();

    virtual TArray<uint8> ReadColorBuffer(UTextureRenderTarget2D* Target) = 0;
    virtual TArray<float> ReadDepthBuffer(UTextureRenderTarget2D* Target) = 0;

protected:
#if PLATFORM_WINDOWS
    // DX12/DX11 specific implementation
    virtual TArray<uint8> ReadColorBuffer_DX(UTextureRenderTarget2D* Target);
#elif PLATFORM_LINUX
    // Vulkan specific implementation
    virtual TArray<uint8> ReadColorBuffer_Vulkan(UTextureRenderTarget2D* Target);
#elif PLATFORM_MAC
    // Metal specific implementation
    virtual TArray<uint8> ReadColorBuffer_Metal(UTextureRenderTarget2D* Target);
#endif
};
```

---

*Document Version: 1.0*
*Last Updated: January 2025*
