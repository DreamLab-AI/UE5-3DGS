# Domain Events

## Overview

Domain Events represent significant occurrences within the domain that domain experts care about. They enable loose coupling between bounded contexts and support event-driven architectures.

---

## Event Base Classes

```cpp
// Shared/Events/DomainEvent.h
#pragma once

#include "CoreMinimal.h"

namespace Shared
{
    /**
     * Base class for all domain events
     */
    struct FDomainEvent
    {
        FDomainEvent()
            : EventId(FGuid::NewGuid())
            , Timestamp(FDateTime::UtcNow())
        {}

        virtual ~FDomainEvent() = default;

        // Event identity
        FGuid EventId;
        FDateTime Timestamp;

        // Source context
        FString ContextName;
        FGuid CorrelationId;  // Links related events
        FGuid CausationId;    // Event that caused this one

        // Serialization
        virtual FString GetEventType() const = 0;
        virtual TSharedPtr<FJsonObject> ToJson() const;

        // Metadata
        TMap<FString, FString> Metadata;
    };

    /**
     * Event handler delegate
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnDomainEvent, const FDomainEvent&);

    /**
     * Event bus for publishing and subscribing to domain events
     */
    class FEventBus
    {
    public:
        static FEventBus& Get();

        // Publishing
        void Publish(const FDomainEvent& Event);

        // Subscription
        template<typename TEvent>
        FDelegateHandle Subscribe(TFunction<void(const TEvent&)> Handler);

        void Unsubscribe(FDelegateHandle Handle);

        // Replay support
        void EnableEventStore(const FString& StorePath);
        TArray<TSharedPtr<FDomainEvent>> GetEventHistory(
            const FGuid& CorrelationId
        ) const;

    private:
        TMap<FString, FOnDomainEvent> EventHandlers;
        TArray<TSharedPtr<FDomainEvent>> EventStore;
        bool bEventStoreEnabled = false;
    };
}
```

---

## Scene Capture Events

### CaptureStarted

Published when a capture session begins.

```cpp
// SCM/Events/SCM_CaptureStartedEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"
#include "SCM_Types.h"

namespace SCM
{
    /**
     * CaptureStarted Event
     *
     * Published when a capture session transitions to Capturing state.
     */
    struct FCaptureStartedEvent : public Shared::FDomainEvent
    {
        FCaptureStartedEvent() = default;

        FCaptureStartedEvent(
            const FGuid& InSessionId,
            const FSCM_CaptureConfiguration& InConfig,
            int32 InExpectedFrameCount
        )
            : SessionId(InSessionId)
            , Configuration(InConfig)
            , ExpectedFrameCount(InExpectedFrameCount)
        {
            ContextName = TEXT("SceneCapture");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("SCM.CaptureStarted");
        }

        virtual TSharedPtr<FJsonObject> ToJson() const override
        {
            auto Json = FDomainEvent::ToJson();
            Json->SetStringField(TEXT("sessionId"), SessionId.ToString());
            Json->SetNumberField(TEXT("expectedFrameCount"), ExpectedFrameCount);
            Json->SetNumberField(TEXT("resolutionX"), Configuration.Resolution.X);
            Json->SetNumberField(TEXT("resolutionY"), Configuration.Resolution.Y);
            return Json;
        }

        // Event data
        FGuid SessionId;
        FSCM_CaptureConfiguration Configuration;
        int32 ExpectedFrameCount;
        FString WorldName;
        FString LevelName;
    };
}
```

### FrameRendered

Published when a single frame completes rendering.

```cpp
// SCM/Events/SCM_FrameRenderedEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"

namespace SCM
{
    /**
     * FrameRendered Event
     *
     * Published when a frame capture completes.
     */
    struct FFrameRenderedEvent : public Shared::FDomainEvent
    {
        FFrameRenderedEvent() = default;

        FFrameRenderedEvent(
            const FGuid& InSessionId,
            int32 InFrameIndex,
            int32 InTotalFrames,
            const FTransform& InCameraTransform
        )
            : SessionId(InSessionId)
            , FrameIndex(InFrameIndex)
            , TotalFrames(InTotalFrames)
            , CameraTransform(InCameraTransform)
        {
            ContextName = TEXT("SceneCapture");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("SCM.FrameRendered");
        }

        // Event data
        FGuid SessionId;
        int32 FrameIndex;
        int32 TotalFrames;
        FTransform CameraTransform;

        // Buffer availability
        bool bHasColorBuffer = true;
        bool bHasDepthBuffer = false;
        bool bHasNormalBuffer = false;

        // Timing
        float RenderTime_ms;

        // Progress
        float GetProgress() const
        {
            return TotalFrames > 0
                ? static_cast<float>(FrameIndex + 1) / TotalFrames
                : 0.0f;
        }
    };
}
```

### CaptureCompleted

Published when capture session finishes successfully.

```cpp
// SCM/Events/SCM_CaptureCompletedEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"

namespace SCM
{
    /**
     * CaptureCompleted Event
     *
     * Published when capture session completes successfully.
     */
    struct FCaptureCompletedEvent : public Shared::FDomainEvent
    {
        FCaptureCompletedEvent() = default;

        FCaptureCompletedEvent(
            const FGuid& InSessionId,
            int32 InFramesCaptured,
            FTimespan InDuration
        )
            : SessionId(InSessionId)
            , FramesCaptured(InFramesCaptured)
            , Duration(InDuration)
        {
            ContextName = TEXT("SceneCapture");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("SCM.CaptureCompleted");
        }

        // Event data
        FGuid SessionId;
        int32 FramesCaptured;
        FTimespan Duration;

        // Statistics
        float AverageFrameTime_ms;
        int64 TotalBytesRendered;
        FBox SceneBounds;
        float CoverageAchieved;
    };
}
```

---

## Data Extraction Events

### BufferExtracted

Published when a render buffer is extracted and processed.

```cpp
// DEM/Events/DEM_BufferExtractedEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"
#include "DEM_Types.h"

namespace DEM
{
    /**
     * BufferExtracted Event
     *
     * Published when a buffer extraction completes.
     */
    struct FBufferExtractedEvent : public Shared::FDomainEvent
    {
        enum class EBufferType : uint8
        {
            Color,
            Depth,
            Normal,
            MotionVector
        };

        FBufferExtractedEvent() = default;

        FBufferExtractedEvent(
            const FGuid& InSceneDataId,
            int32 InCameraIndex,
            EBufferType InBufferType
        )
            : SceneDataId(InSceneDataId)
            , CameraIndex(InCameraIndex)
            , BufferType(InBufferType)
        {
            ContextName = TEXT("DataExtraction");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("DEM.BufferExtracted");
        }

        // Event data
        FGuid SceneDataId;
        int32 CameraIndex;
        EBufferType BufferType;

        // Buffer info
        FIntPoint Resolution;
        EBufferFormat Format;
        int64 SizeBytes;

        // Processing info
        EDEM_CoordinateConvention SourceConvention;
        EDEM_CoordinateConvention TargetConvention;
    };
}
```

### SceneDataReady

Published when all extraction for a scene completes.

```cpp
// DEM/Events/DEM_SceneDataReadyEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"

namespace DEM
{
    /**
     * SceneDataReady Event
     *
     * Published when scene data extraction is complete and validated.
     */
    struct FSceneDataReadyEvent : public Shared::FDomainEvent
    {
        FSceneDataReadyEvent() = default;

        FSceneDataReadyEvent(
            const FGuid& InSceneDataId,
            const FGuid& InSourceSessionId
        )
            : SceneDataId(InSceneDataId)
            , SourceSessionId(InSourceSessionId)
        {
            ContextName = TEXT("DataExtraction");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("DEM.SceneDataReady");
        }

        // Event data
        FGuid SceneDataId;
        FGuid SourceSessionId;

        // Scene statistics
        int32 CameraCount;
        int32 ColorBufferCount;
        int32 DepthBufferCount;
        FBox SceneBounds;

        // Validation
        bool bValidationPassed;
        TArray<FString> ValidationWarnings;
    };
}
```

---

## Format Conversion Events

### ExportStarted

Published when export job begins.

```cpp
// FCM/Events/FCM_ExportStartedEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"
#include "FCM_Types.h"

namespace FCM
{
    /**
     * ExportStarted Event
     *
     * Published when an export job begins execution.
     */
    struct FExportStartedEvent : public Shared::FDomainEvent
    {
        FExportStartedEvent() = default;

        FExportStartedEvent(
            const FGuid& InJobId,
            EFFCM_ExportFormat InFormat,
            const FString& InOutputPath
        )
            : JobId(InJobId)
            , Format(InFormat)
            , OutputPath(InOutputPath)
        {
            ContextName = TEXT("FormatConversion");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("FCM.ExportStarted");
        }

        // Event data
        FGuid JobId;
        EFFCM_ExportFormat Format;
        FString OutputPath;

        // Export scope
        int32 ImageCount;
        int32 CameraCount;
        bool bIncludeDepth;
        bool bIncludeNormals;
        bool bGenerateManifest;
    };
}
```

### ExportCompleted

Published when export finishes successfully.

```cpp
// FCM/Events/FCM_ExportCompletedEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"
#include "FCM_Types.h"

namespace FCM
{
    /**
     * ExportCompleted Event
     *
     * Published when an export job completes successfully.
     */
    struct FExportCompletedEvent : public Shared::FDomainEvent
    {
        FExportCompletedEvent() = default;

        FExportCompletedEvent(
            const FGuid& InJobId,
            const FFCM_ExportResult& InResult
        )
            : JobId(InJobId)
            , Result(InResult)
        {
            ContextName = TEXT("FormatConversion");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("FCM.ExportCompleted");
        }

        // Event data
        FGuid JobId;
        FFCM_ExportResult Result;

        // Output paths
        FString GetImagesPath() const { return Result.ImagesPath; }
        FString GetCOLMAPPath() const { return Result.COLMAPPath; }
        FString GetManifestPath() const { return Result.ManifestPath; }
    };
}
```

---

## Training Events

### TrainingStarted

Published when training session begins.

```cpp
// TRN/Events/TRN_TrainingStartedEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"
#include "TRN_Types.h"

namespace TRN
{
    /**
     * TrainingStarted Event
     *
     * Published when 3DGS training begins.
     */
    struct FTrainingStartedEvent : public Shared::FDomainEvent
    {
        FTrainingStartedEvent() = default;

        FTrainingStartedEvent(
            const FGuid& InSessionId,
            const FTRN_TrainingConfig& InConfig,
            int32 InInitialGaussianCount
        )
            : SessionId(InSessionId)
            , Config(InConfig)
            , InitialGaussianCount(InInitialGaussianCount)
        {
            ContextName = TEXT("Training");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("TRN.TrainingStarted");
        }

        // Event data
        FGuid SessionId;
        FTRN_TrainingConfig Config;
        int32 InitialGaussianCount;

        // Hardware info
        FString GPUName;
        int64 GPUMemoryBytes;
    };
}
```

### TrainingProgressed

Published periodically during training.

```cpp
// TRN/Events/TRN_TrainingProgressedEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"

namespace TRN
{
    /**
     * TrainingProgressed Event
     *
     * Published periodically during training with metrics.
     */
    struct FTrainingProgressedEvent : public Shared::FDomainEvent
    {
        FTrainingProgressedEvent() = default;

        FTrainingProgressedEvent(
            const FGuid& InSessionId,
            int32 InIteration,
            int32 InTotalIterations
        )
            : SessionId(InSessionId)
            , Iteration(InIteration)
            , TotalIterations(InTotalIterations)
        {
            ContextName = TEXT("Training");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("TRN.TrainingProgressed");
        }

        // Event data
        FGuid SessionId;
        int32 Iteration;
        int32 TotalIterations;

        // Metrics
        float Loss;
        float PSNR;
        float SSIM;

        // Model state
        int32 GaussianCount;
        int32 GaussiansAdded;
        int32 GaussiansPruned;

        // Performance
        float IterationTime_ms;
        int64 GPUMemoryUsed;

        float GetProgress() const
        {
            return TotalIterations > 0
                ? static_cast<float>(Iteration) / TotalIterations
                : 0.0f;
        }
    };
}
```

### TrainingCompleted

Published when training finishes.

```cpp
// TRN/Events/TRN_TrainingCompletedEvent.h
#pragma once

#include "Shared/Events/DomainEvent.h"

namespace TRN
{
    /**
     * TrainingCompleted Event
     *
     * Published when 3DGS training completes.
     */
    struct FTrainingCompletedEvent : public Shared::FDomainEvent
    {
        FTrainingCompletedEvent() = default;

        FTrainingCompletedEvent(
            const FGuid& InSessionId,
            bool bInSuccess
        )
            : SessionId(InSessionId)
            , bSuccess(bInSuccess)
        {
            ContextName = TEXT("Training");
        }

        virtual FString GetEventType() const override
        {
            return TEXT("TRN.TrainingCompleted");
        }

        // Event data
        FGuid SessionId;
        bool bSuccess;
        FString FailureReason;

        // Final metrics
        float FinalLoss;
        float FinalPSNR;
        float FinalSSIM;

        // Model statistics
        int32 FinalGaussianCount;
        FTimespan TrainingDuration;
        int32 TotalIterations;

        // Output
        FString OutputPath;
    };
}
```

---

## Event Handling Patterns

### Event Subscription Example

```cpp
// Subscribe to events across contexts
void FPipelineOrchestrator::SetupEventHandlers()
{
    auto& EventBus = Shared::FEventBus::Get();

    // Handle capture completion
    EventBus.Subscribe<SCM::FCaptureCompletedEvent>(
        [this](const SCM::FCaptureCompletedEvent& Event)
        {
            OnCaptureCompleted(Event);
        }
    );

    // Handle scene data ready
    EventBus.Subscribe<DEM::FSceneDataReadyEvent>(
        [this](const DEM::FSceneDataReadyEvent& Event)
        {
            OnSceneDataReady(Event);
        }
    );

    // Handle export completion
    EventBus.Subscribe<FCM::FExportCompletedEvent>(
        [this](const FCM::FExportCompletedEvent& Event)
        {
            OnExportCompleted(Event);
        }
    );
}

void FPipelineOrchestrator::OnCaptureCompleted(
    const SCM::FCaptureCompletedEvent& Event
)
{
    // Trigger data extraction
    auto SceneData = ExtractionContext->CreateSceneData(
        *CaptureContext->GetSession(Event.SessionId)
    );

    // Continue pipeline...
}
```

### Event-Driven Pipeline

```cpp
// Event-driven pipeline orchestration
class FEventDrivenPipeline
{
public:
    void Start(const FSCM_CaptureConfiguration& Config)
    {
        // Start capture - events will drive the pipeline
        auto Session = CaptureContext->CreateSession(World, Config);
        Session->OnStateChanged.AddLambda(
            [this](ECaptureState Old, ECaptureState New)
            {
                if (New == ECaptureState::Completed)
                {
                    // Capture done, extraction starts automatically via events
                }
            }
        );

        Session->Start();
    }

private:
    void HandleEvent(const Shared::FDomainEvent& Event)
    {
        if (Event.GetEventType() == TEXT("SCM.CaptureCompleted"))
        {
            StartExtraction(static_cast<const SCM::FCaptureCompletedEvent&>(Event));
        }
        else if (Event.GetEventType() == TEXT("DEM.SceneDataReady"))
        {
            StartExport(static_cast<const DEM::FSceneDataReadyEvent&>(Event));
        }
        else if (Event.GetEventType() == TEXT("FCM.ExportCompleted"))
        {
            OnPipelineComplete(static_cast<const FCM::FExportCompletedEvent&>(Event));
        }
    }
};
```
