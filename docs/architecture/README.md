# Architecture Guide

[← Back to Main README](../../README.md) | [API Reference →](../api/README.md)

---

## System Architecture Overview

UE5-3DGS follows a modular pipeline architecture designed for extensibility, testability, and production reliability.

```mermaid
flowchart TB
    subgraph Input["Input Layer"]
        UE5Scene["UE5 Scene"]
        CameraComp["Camera Component"]
        UserConfig["User Configuration"]
    end

    subgraph Core["Core Processing Layer"]
        direction TB
        CoordConv["Coordinate Converter"]
        CamIntrinsics["Camera Intrinsics"]
        TrajGen["Trajectory Generator"]
        DepthExt["Depth Extractor"]
    end

    subgraph Orchestration["Orchestration Layer"]
        Orch["Capture Orchestrator"]
        EditorMode["Editor Mode"]
        Validation["Output Validation"]
    end

    subgraph Export["Export Layer"]
        ColmapWriter["COLMAP Writer"]
        PlyExporter["PLY Exporter"]
        ImageWriter["Image Writer"]
    end

    subgraph Output["Output Formats"]
        COLMAP_TXT["COLMAP Text"]
        COLMAP_BIN["COLMAP Binary"]
        PLY_3DGS["3DGS PLY"]
        Images["Training Images"]
    end

    Input --> Core
    Core --> Orchestration
    Orchestration --> Export
    Export --> Output

    style Input fill:#3498db,color:#fff
    style Core fill:#e74c3c,color:#fff
    style Orchestration fill:#9b59b6,color:#fff
    style Export fill:#2ecc71,color:#fff
    style Output fill:#f39c12,color:#fff
```

---

## Module Dependency Graph

```mermaid
graph TD
    subgraph Public["Public Headers"]
        UE5_3DGS_H["UE5_3DGS.h"]
        Types_H["Types.h"]
        Settings_H["CaptureSettings.h"]
    end

    subgraph Core["Core Module"]
        CoordConv["CoordinateConverter.h/.cpp"]
        CamInt["CameraIntrinsics.h/.cpp"]
        TrajGen["TrajectoryGenerator.h/.cpp"]
        DepthExt["DepthExtractor.h/.cpp"]
    end

    subgraph FCM["Format Compliance Module"]
        ColmapWriter["ColmapWriter.h/.cpp"]
        PlyExporter["PlyExporter.h/.cpp"]
    end

    subgraph Orchestration["Orchestration Module"]
        CaptureOrch["CaptureOrchestrator.h/.cpp"]
    end

    subgraph Editor["Editor Module"]
        EditorMode["GaussianCaptureEditorMode.h/.cpp"]
        Widgets["UI Widgets"]
    end

    UE5_3DGS_H --> Types_H
    UE5_3DGS_H --> Settings_H

    CaptureOrch --> CoordConv
    CaptureOrch --> CamInt
    CaptureOrch --> TrajGen
    CaptureOrch --> DepthExt
    CaptureOrch --> ColmapWriter
    CaptureOrch --> PlyExporter

    EditorMode --> CaptureOrch
    EditorMode --> Widgets

    ColmapWriter --> CoordConv
    ColmapWriter --> CamInt
    PlyExporter --> CoordConv
```

---

## Data Flow Pipeline

```mermaid
sequenceDiagram
    autonumber
    participant User
    participant Editor as Editor Mode
    participant Orch as CaptureOrchestrator
    participant Traj as TrajectoryGenerator
    participant Scene as Scene Capture
    participant Conv as CoordinateConverter
    participant Writer as Format Writers

    User->>Editor: Configure Settings
    Editor->>Orch: Initialize(Settings)
    Orch->>Traj: GenerateTrajectory(Type, Count)
    Traj-->>Orch: TArray<FTransform>

    loop For Each View
        Orch->>Scene: SetViewTransform(Transform)
        Orch->>Scene: CaptureScene()
        Scene-->>Orch: RenderTarget Data
        Orch->>Conv: ConvertPose(Transform)
        Conv-->>Orch: COLMAP Pose
        Orch->>Writer: BufferFrame(Image, Pose)
    end

    Orch->>Writer: Flush()
    Writer-->>Orch: Export Complete
    Orch-->>User: OnCaptureComplete
```

---

## Class Hierarchy

```mermaid
classDiagram
    class UObject {
        <<UE5 Base>>
    }

    class UCaptureOrchestrator {
        +FCaptureSettings Settings
        +TArray~FTransform~ Trajectory
        +Initialize(Settings)
        +StartCapture()
        +StopCapture()
        +GetProgress() float
        #ProcessFrame()
        #WriteOutput()
    }

    class FCoordinateConverter {
        <<Static Utility>>
        +UE5ToColmap(FVector)$ FVector
        +ColmapToUE5(FVector)$ FVector
        +UE5ToColmapRotation(FQuat)$ FQuat
        +ConvertTransform(FTransform)$ FTransform
        -ScaleFactor$ float
    }

    class FCameraIntrinsics {
        +int32 Width
        +int32 Height
        +float FocalLengthX
        +float FocalLengthY
        +float PrincipalPointX
        +float PrincipalPointY
        +ECameraModel Model
        +TArray~float~ DistortionCoeffs
        +ExtractFromComponent(UCameraComponent*)
        +GetColmapModelName() FString
        +GetColmapParamsString() FString
    }

    class FTrajectoryGenerator {
        <<Static Factory>>
        +GenerateOrbital(...)$ TArray~FTransform~
        +GenerateSpiral(...)$ TArray~FTransform~
        +GenerateHemisphere(...)$ TArray~FTransform~
        +Generate360(...)$ TArray~FTransform~
        +InterpolateKeyframes(...)$ TArray~FTransform~
    }

    class FColmapWriter {
        +WriteCamerasTxt(Path, Cameras)
        +WriteImagesTxt(Path, Images)
        +WritePoints3DTxt(Path, Points)
        +WriteCamerasBin(Path, Cameras)
        +WriteImagesBin(Path, Images)
        +CreateCamera(Intrinsics, Id)$ FColmapCamera
    }

    class FPlyExporter {
        +ExportGaussians(Path, Splats) bool
        #WriteHeader(Archive, Count)
        #WritePoint(Archive, Splat)
        -SHCoeffC0$ float
    }

    class FDepthExtractor {
        +USceneCaptureComponent2D* CaptureComponent
        +Initialize(Width, Height)
        +ExtractDepth() TArray~float~
        +ConvertToPointCloud() TArray~FVector~
    }

    UObject <|-- UCaptureOrchestrator
    UCaptureOrchestrator --> FCoordinateConverter
    UCaptureOrchestrator --> FCameraIntrinsics
    UCaptureOrchestrator --> FTrajectoryGenerator
    UCaptureOrchestrator --> FColmapWriter
    UCaptureOrchestrator --> FPlyExporter
    UCaptureOrchestrator --> FDepthExtractor
```

---

## State Machine: Capture Process

```mermaid
stateDiagram-v2
    [*] --> Idle

    Idle --> Initializing: Initialize()
    Initializing --> Ready: Success
    Initializing --> Error: Failed

    Ready --> Capturing: StartCapture()
    Capturing --> Processing: Frame Ready
    Processing --> Capturing: Continue
    Processing --> Writing: All Frames Done

    Capturing --> Paused: PauseCapture()
    Paused --> Capturing: ResumeCapture()
    Paused --> Idle: StopCapture()

    Writing --> Validating: Write Complete
    Validating --> Complete: Valid
    Validating --> Error: Invalid

    Complete --> Idle: Reset()
    Error --> Idle: Reset()

    Capturing --> Error: Exception
    Processing --> Error: Exception
    Writing --> Error: Exception
```

---

## Component Interaction: Editor Mode

```mermaid
flowchart LR
    subgraph EditorUI["Editor UI"]
        Panel["Capture Panel"]
        Props["Property Editor"]
        Viewport["Viewport Overlay"]
    end

    subgraph EditorMode["FGaussianCaptureEditorMode"]
        Gizmos["Camera Gizmos"]
        Preview["Preview System"]
        Validate["Validation"]
    end

    subgraph Backend["Backend Services"]
        Orch["CaptureOrchestrator"]
        Assets["Asset Manager"]
        Notif["Notification System"]
    end

    Panel --> EditorMode
    Props --> EditorMode
    EditorMode --> Gizmos
    EditorMode --> Preview
    EditorMode --> Validate
    Gizmos --> Viewport
    Preview --> Viewport
    EditorMode --> Orch
    EditorMode --> Assets
    Validate --> Notif
```

---

## Memory Management

```mermaid
flowchart TB
    subgraph Allocation["Memory Allocation Strategy"]
        direction TB
        Pool["Object Pool<br/>Reusable Frame Buffers"]
        Ring["Ring Buffer<br/>Streaming Images"]
        Stack["Stack Allocator<br/>Temporary Transforms"]
    end

    subgraph Lifecycle["Object Lifecycle"]
        direction TB
        Create["Allocate on Init"]
        Use["Process Frames"]
        Recycle["Return to Pool"]
        Destroy["Release on Shutdown"]
    end

    Pool --> Create
    Create --> Use
    Use --> Recycle
    Recycle --> Use
    Use --> Destroy
```

**Key Considerations:**
- Frame buffers pooled to avoid allocation during capture
- Ring buffer for image streaming to disk
- RAII patterns for resource management
- Weak references for UObject interactions

---

## Threading Model

```mermaid
flowchart TB
    subgraph GameThread["Game Thread"]
        GT_Tick["Tick()"]
        GT_UI["UI Updates"]
        GT_Transform["Transform Updates"]
    end

    subgraph RenderThread["Render Thread"]
        RT_Capture["Scene Capture"]
        RT_Depth["Depth Extraction"]
        RT_ReadBack["GPU Readback"]
    end

    subgraph AsyncTasks["Async Task Pool"]
        AT_Convert["Coordinate Convert"]
        AT_Write["File I/O"]
        AT_Compress["Image Compression"]
    end

    GT_Tick --> RT_Capture
    RT_Capture --> RT_Depth
    RT_Depth --> RT_ReadBack
    RT_ReadBack --> AT_Convert
    AT_Convert --> AT_Write
    AT_Write --> AT_Compress
    AT_Compress --> GT_UI
```

---

## Error Handling Architecture

```mermaid
flowchart TB
    subgraph ErrorSources["Error Sources"]
        IO["File I/O Errors"]
        GPU["GPU Errors"]
        Valid["Validation Errors"]
        Config["Configuration Errors"]
    end

    subgraph ErrorHandler["Error Handler"]
        Catch["Try/Catch Blocks"]
        Log["Logging System"]
        Delegate["Error Delegates"]
    end

    subgraph Recovery["Recovery Actions"]
        Retry["Retry Operation"]
        Skip["Skip Frame"]
        Abort["Abort Capture"]
        Notify["User Notification"]
    end

    IO --> Catch
    GPU --> Catch
    Valid --> Catch
    Config --> Catch

    Catch --> Log
    Catch --> Delegate

    Log --> Retry
    Log --> Skip
    Delegate --> Abort
    Delegate --> Notify
```

---

## Extension Points

| Extension Point | Description | Implementation |
|-----------------|-------------|----------------|
| Custom Trajectory | User-defined camera paths | Implement `ITrajectoryProvider` |
| Export Format | New output formats | Extend `FExportWriter` |
| Validation Rules | Custom validation | Add to `FOutputValidator` |
| Post-Processing | Image filters | Register with `FCapturePostProcess` |

---

## Related Documentation

| Document | Description |
|----------|-------------|
| [API Reference](../api/README.md) | Complete API documentation |
| [Coordinate Systems](../reference/coordinates.md) | Transformation mathematics |
| [Format Specifications](../reference/formats.md) | COLMAP and PLY details |
| [User Guide](../guides/user-guide.md) | Step-by-step usage |

---

[← Back to Main README](../../README.md) | [API Reference →](../api/README.md)
