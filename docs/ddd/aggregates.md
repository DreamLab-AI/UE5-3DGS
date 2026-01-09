# Aggregates

## Overview

Aggregates are clusters of domain objects treated as single units for data changes. Each aggregate has a root entity that controls access to all objects within the boundary.

---

## 1. CaptureSession Aggregate

### Aggregate Root: `FSCM_CaptureSession`

The CaptureSession is the primary aggregate for managing capture workflows. It maintains consistency of all captured frames and ensures trajectory execution integrity.

```cpp
// SCM/Aggregates/SCM_CaptureSession.h
#pragma once

#include "CoreMinimal.h"
#include "SCM_CapturedFrame.h"
#include "SCM_Trajectory.h"

namespace SCM
{
    /**
     * Capture session state enumeration
     */
    enum class ECaptureState : uint8
    {
        Initialising,
        Ready,
        Capturing,
        Paused,
        Completed,
        Failed,
        Cancelled
    };

    /**
     * CaptureSession Aggregate Root
     *
     * Invariants:
     * - Frames must be added in sequential order
     * - Cannot add frames when state is not Capturing
     * - Trajectory cannot be modified after capture starts
     * - All frames must belong to the same world
     */
    class FSCM_CaptureSession
    {
    public:
        FSCM_CaptureSession(
            UWorld* InWorld,
            const FSCM_CaptureConfiguration& InConfig
        );

        // Identity
        const FGuid& GetSessionId() const { return SessionId; }

        // State management
        ECaptureState GetState() const { return State; }
        bool CanStart() const;
        bool CanPause() const;
        bool CanResume() const;
        bool CanComplete() const;

        // Lifecycle
        void Start();
        void Pause();
        void Resume();
        void Complete();
        void Cancel(const FString& Reason);
        void Fail(const FString& Error);

        // Frame management (only via aggregate root)
        void AddFrame(TSharedPtr<FSCM_CapturedFrame> Frame);
        int32 GetFrameCount() const { return Frames.Num(); }
        TSharedPtr<FSCM_CapturedFrame> GetFrame(int32 Index) const;
        const TArray<TSharedPtr<FSCM_CapturedFrame>>& GetAllFrames() const;

        // Trajectory (immutable after start)
        void SetTrajectory(TSharedPtr<FSCM_Trajectory> InTrajectory);
        TSharedPtr<FSCM_Trajectory> GetTrajectory() const;

        // Progress
        float GetProgress() const;
        int32 GetExpectedFrameCount() const;

        // Configuration (read-only after creation)
        const FSCM_CaptureConfiguration& GetConfiguration() const;

        // World reference
        UWorld* GetWorld() const { return World.Get(); }

        // Events
        DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStateChanged, ECaptureState, ECaptureState);
        DECLARE_MULTICAST_DELEGATE_OneParam(FOnFrameAdded, TSharedPtr<FSCM_CapturedFrame>);
        DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProgress, int32, int32);

        FOnStateChanged OnStateChanged;
        FOnFrameAdded OnFrameAdded;
        FOnProgress OnProgress;

    private:
        void TransitionTo(ECaptureState NewState);
        void ValidateFrame(const FSCM_CapturedFrame& Frame) const;

        FGuid SessionId;
        TWeakObjectPtr<UWorld> World;
        FSCM_CaptureConfiguration Configuration;
        ECaptureState State = ECaptureState::Initialising;

        TSharedPtr<FSCM_Trajectory> Trajectory;
        TArray<TSharedPtr<FSCM_CapturedFrame>> Frames;

        FDateTime StartTime;
        FDateTime EndTime;
        FString FailureReason;
    };
}
```

### Invariant Rules

1. **Frame Ordering**: Frames must be added with sequential `FrameIndex`
2. **State Machine**: Valid state transitions are enforced
3. **Trajectory Immutability**: Trajectory locked once capture starts
4. **World Consistency**: All frames captured from the same world

### State Machine

```
                    ┌──────────────┐
                    │ Initialising │
                    └──────┬───────┘
                           │ SetTrajectory()
                           ▼
                    ┌──────────────┐
                    │    Ready     │
                    └──────┬───────┘
                           │ Start()
                           ▼
         ┌────────────────────────────────┐
         │                                │
         ▼                                │
  ┌──────────────┐                        │
  │  Capturing   │◀────────┐              │
  └──────┬───────┘         │              │
         │                 │              │
    ┌────┴────┐       Resume()            │
    │         │            │              │
Pause()   Complete()       │              │
    │         │            │              │
    ▼         ▼            │              │
┌────────┐ ┌───────────┐   │              │
│ Paused │─┤ Completed │   │              │
└────────┘ └───────────┘   │              │
                           │              │
         Cancel()/Fail()   │     Cancel() │
              │            │              │
              └────────────┼──────────────┘
                           │
                           ▼
                    ┌──────────────┐
                    │Failed/Cancel │
                    └──────────────┘
```

---

## 2. SceneData Aggregate

### Aggregate Root: `FDEM_SceneData`

SceneData aggregates all extracted information from a capture session, including camera parameters, buffers, and computed metadata.

```cpp
// DEM/Aggregates/DEM_SceneData.h
#pragma once

#include "CoreMinimal.h"
#include "DEM_CameraParameters.h"
#include "DEM_RenderBuffer.h"
#include "DEM_SceneMetadata.h"

namespace DEM
{
    /**
     * SceneData Aggregate Root
     *
     * Invariants:
     * - Camera count must match image count
     * - All buffers must have consistent dimensions
     * - Bounds must be recalculated when cameras change
     * - Metadata must be consistent with camera/buffer state
     */
    class FDEM_SceneData
    {
    public:
        FDEM_SceneData(const FGuid& SourceSessionId);

        // Identity
        const FGuid& GetSceneId() const { return SceneId; }
        const FGuid& GetSourceSessionId() const { return SourceSessionId; }

        // Camera parameters (managed via aggregate)
        void AddCamera(TSharedPtr<FDEM_CameraParameters> Camera);
        int32 GetCameraCount() const { return Cameras.Num(); }
        TSharedPtr<FDEM_CameraParameters> GetCamera(int32 Index) const;
        const TArray<TSharedPtr<FDEM_CameraParameters>>& GetAllCameras() const;

        // Render buffers (managed via aggregate)
        void AddColorBuffer(int32 CameraIndex, TSharedPtr<FDEM_ColorBuffer> Buffer);
        void AddDepthBuffer(int32 CameraIndex, TSharedPtr<FDEM_DepthBuffer> Buffer);
        void AddNormalBuffer(int32 CameraIndex, TSharedPtr<FDEM_NormalBuffer> Buffer);

        TSharedPtr<FDEM_ColorBuffer> GetColorBuffer(int32 CameraIndex) const;
        TSharedPtr<FDEM_DepthBuffer> GetDepthBuffer(int32 CameraIndex) const;
        TSharedPtr<FDEM_NormalBuffer> GetNormalBuffer(int32 CameraIndex) const;

        // Computed properties
        FBox GetSceneBounds() const;
        FVector GetSceneCentroid() const;
        float GetSceneScale() const;

        // Metadata
        const FDEM_SceneMetadata& GetMetadata() const { return Metadata; }
        void UpdateMetadata(const FDEM_SceneMetadata& NewMetadata);

        // Validation
        bool Validate() const;
        TArray<FString> GetValidationErrors() const;

        // Statistics
        int32 GetTotalBufferSize() const;
        FIntPoint GetResolution() const;

    private:
        void RecalculateBounds();
        void ValidateConsistency() const;

        FGuid SceneId;
        FGuid SourceSessionId;

        TArray<TSharedPtr<FDEM_CameraParameters>> Cameras;
        TMap<int32, TSharedPtr<FDEM_ColorBuffer>> ColorBuffers;
        TMap<int32, TSharedPtr<FDEM_DepthBuffer>> DepthBuffers;
        TMap<int32, TSharedPtr<FDEM_NormalBuffer>> NormalBuffers;

        FDEM_SceneMetadata Metadata;
        FBox CachedBounds;
        bool bBoundsDirty = true;
    };

    /**
     * Scene metadata structure
     */
    struct FDEM_SceneMetadata
    {
        FString SceneName;
        FString UE5Project;
        FString UE5Level;

        FVector UpAxis = FVector::UpVector;
        float ScaleMetres = 0.01f;  // UE5 uses cm

        FDateTime CaptureTimestamp;
        FString CaptureConfiguration;
    };
}
```

### Invariant Rules

1. **Camera-Buffer Consistency**: Buffer count must match camera count per type
2. **Resolution Consistency**: All buffers must share the same resolution
3. **Bounds Validity**: Scene bounds recalculated on camera changes
4. **Metadata Sync**: Metadata reflects current aggregate state

---

## 3. ExportJob Aggregate

### Aggregate Root: `FFCM_ExportJob`

ExportJob manages the asynchronous export process with progress tracking and cancellation support.

```cpp
// FCM/Aggregates/FCM_ExportJob.h
#pragma once

#include "CoreMinimal.h"
#include "FCM_Types.h"
#include "Async/Future.h"

namespace FCM
{
    /**
     * Export job state enumeration
     */
    enum class EExportState : uint8
    {
        Pending,
        Running,
        Completed,
        Failed,
        Cancelled
    };

    /**
     * ExportJob Aggregate Root
     *
     * Invariants:
     * - Job cannot be started twice
     * - Cancellation only valid when Running
     * - Results only available when Completed
     * - Progress monotonically increases
     */
    class FFCM_ExportJob
    {
    public:
        FFCM_ExportJob(const FFCM_ExportConfiguration& InConfig);

        // Identity
        const FGuid& GetJobId() const { return JobId; }

        // State
        EExportState GetState() const { return State; }
        bool IsRunning() const { return State == EExportState::Running; }
        bool IsComplete() const { return State == EExportState::Completed; }

        // Execution
        TFuture<FFCM_ExportResult> ExecuteAsync(
            const DEM::FDEM_SceneData& SceneData
        );

        void Cancel();

        // Progress (0.0 to 1.0)
        float GetProgress() const { return Progress; }
        FString GetCurrentOperation() const { return CurrentOperation; }

        // Results (only valid when Complete)
        TOptional<FFCM_ExportResult> GetResult() const;

        // Configuration
        const FFCM_ExportConfiguration& GetConfiguration() const;

        // Events
        DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProgressUpdated, float, const FString&);
        DECLARE_MULTICAST_DELEGATE_OneParam(FOnStateChanged, EExportState);
        DECLARE_MULTICAST_DELEGATE_OneParam(FOnCompleted, const FFCM_ExportResult&);

        FOnProgressUpdated OnProgressUpdated;
        FOnStateChanged OnStateChanged;
        FOnCompleted OnCompleted;

    private:
        void TransitionTo(EExportState NewState);
        void UpdateProgress(float NewProgress, const FString& Operation);

        // Internal export steps
        void WriteImages(const DEM::FDEM_SceneData& SceneData);
        void WriteCOLMAP(const DEM::FDEM_SceneData& SceneData);
        void WriteManifest(const DEM::FDEM_SceneData& SceneData);

        FGuid JobId;
        FFCM_ExportConfiguration Configuration;
        EExportState State = EExportState::Pending;

        TAtomic<float> Progress{0.0f};
        FString CurrentOperation;

        TOptional<FFCM_ExportResult> Result;
        bool bCancellationRequested = false;

        FCriticalSection StateLock;
    };

    /**
     * Export result structure
     */
    struct FFCM_ExportResult
    {
        bool bSuccess;
        FString OutputPath;
        FString ErrorMessage;

        int32 ImagesWritten;
        int32 CamerasWritten;
        int64 TotalBytesWritten;

        FTimespan Duration;

        // File paths
        FString ImagesPath;
        FString COLMAPPath;
        FString ManifestPath;
    };
}
```

### Invariant Rules

1. **Single Execution**: Execute can only be called once per job
2. **Cancellation Window**: Cancel only valid during Running state
3. **Progress Monotonicity**: Progress never decreases
4. **Result Availability**: Results only accessible after completion

---

## 4. TrainingConfig Aggregate

### Aggregate Root: `FTRN_TrainingConfig`

TrainingConfig manages training hyperparameters and creates validated training sessions.

```cpp
// TRN/Aggregates/TRN_TrainingConfig.h
#pragma once

#include "CoreMinimal.h"
#include "TRN_Types.h"

namespace TRN
{
    /**
     * TrainingConfig Aggregate Root
     *
     * Invariants:
     * - Learning rates must be positive
     * - Iteration counts must be consistent (densify_from < densify_until < total)
     * - Thresholds must be in valid ranges
     * - Config is immutable once training starts
     */
    class FTRN_TrainingConfig
    {
    public:
        FTRN_TrainingConfig();

        // Identity
        const FGuid& GetConfigId() const { return ConfigId; }

        // Builder pattern for configuration
        FTRN_TrainingConfig& SetTotalIterations(int32 Value);
        FTRN_TrainingConfig& SetWarmupIterations(int32 Value);

        FTRN_TrainingConfig& SetPositionLearningRate(float Initial, float Final);
        FTRN_TrainingConfig& SetFeatureLearningRate(float Value);
        FTRN_TrainingConfig& SetOpacityLearningRate(float Value);
        FTRN_TrainingConfig& SetScalingLearningRate(float Value);
        FTRN_TrainingConfig& SetRotationLearningRate(float Value);

        FTRN_TrainingConfig& SetDensificationRange(int32 FromIter, int32 UntilIter);
        FTRN_TrainingConfig& SetDensificationInterval(int32 Interval);
        FTRN_TrainingConfig& SetDensifyGradThreshold(float Threshold);

        FTRN_TrainingConfig& SetOpacityPruneThreshold(float Threshold);
        FTRN_TrainingConfig& SetScalePruneThreshold(float Threshold);

        FTRN_TrainingConfig& SetLossWeights(float L1Weight, float SSIMWeight);

        // Validation
        bool Validate() const;
        TArray<FString> GetValidationErrors() const;

        // Session creation
        TSharedPtr<FTRN_TrainingSession> CreateSession(
            const FCM::FFCM_ExportResult& TrainingData
        );

        // Accessors
        int32 GetTotalIterations() const { return TotalIterations; }
        int32 GetWarmupIterations() const { return WarmupIterations; }
        float GetPositionLRInitial() const { return PositionLR_Initial; }
        float GetPositionLRFinal() const { return PositionLR_Final; }
        // ... additional accessors

        // Presets
        static FTRN_TrainingConfig CreateDefault();
        static FTRN_TrainingConfig CreateFast();
        static FTRN_TrainingConfig CreateHighQuality();

    private:
        FGuid ConfigId;

        // Iteration parameters
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

### Invariant Rules

1. **Learning Rate Validity**: All rates must be positive
2. **Iteration Consistency**: `Warmup < DensifyFrom < DensifyUntil < Total`
3. **Threshold Ranges**: Thresholds within valid bounds (e.g., opacity 0-1)
4. **Loss Weight Sum**: L1 + SSIM weights must equal 1.0

---

## Aggregate Interaction Patterns

### Factory Methods

```cpp
// Aggregate factories enforce invariants at creation
class FAggregateFactory
{
public:
    static TSharedPtr<FSCM_CaptureSession> CreateCaptureSession(
        UWorld* World,
        const FSCM_CaptureConfiguration& Config
    )
    {
        // Validate world
        if (!World || !World->IsValidLowLevel())
        {
            return nullptr;
        }

        // Validate configuration
        if (Config.Resolution.X <= 0 || Config.Resolution.Y <= 0)
        {
            return nullptr;
        }

        return MakeShared<FSCM_CaptureSession>(World, Config);
    }
};
```

### Event Sourcing Pattern

```cpp
// Aggregate events for persistence/replay
struct FCaptureSessionEvent : public Shared::FDomainEvent
{
    FGuid SessionId;
};

struct FFrameAddedEvent : public FCaptureSessionEvent
{
    int32 FrameIndex;
    FTransform CameraTransform;
};

struct FStateChangedEvent : public FCaptureSessionEvent
{
    ECaptureState OldState;
    ECaptureState NewState;
};
```
