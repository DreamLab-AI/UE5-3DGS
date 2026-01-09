# Repositories

## Overview

Repositories provide collection-like interfaces for accessing and persisting aggregates. They abstract the underlying storage mechanism and maintain aggregate boundaries.

---

## Repository Base Interface

```cpp
// Shared/Repositories/IRepository.h
#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"

namespace Shared
{
    /**
     * Query specification for filtering repository results
     */
    template<typename TEntity>
    struct TQuerySpec
    {
        TFunction<bool(const TEntity&)> Predicate;
        TOptional<int32> Limit;
        TOptional<int32> Offset;
        TOptional<FString> OrderBy;
        bool bAscending = true;
    };

    /**
     * Base repository interface
     */
    template<typename TAggregate, typename TId>
    class IRepository
    {
    public:
        virtual ~IRepository() = default;

        // Core CRUD operations
        virtual TSharedPtr<TAggregate> FindById(const TId& Id) const = 0;
        virtual TArray<TSharedPtr<TAggregate>> FindAll() const = 0;
        virtual void Save(TSharedPtr<TAggregate> Aggregate) = 0;
        virtual void Remove(const TId& Id) = 0;

        // Query support
        virtual TArray<TSharedPtr<TAggregate>> FindBySpec(
            const TQuerySpec<TAggregate>& Spec
        ) const = 0;

        // Existence check
        virtual bool Exists(const TId& Id) const = 0;
        virtual int32 Count() const = 0;

        // Async variants
        virtual TFuture<TSharedPtr<TAggregate>> FindByIdAsync(const TId& Id) const
        {
            return Async(EAsyncExecution::ThreadPool, [this, Id]()
            {
                return FindById(Id);
            });
        }

        virtual TFuture<void> SaveAsync(TSharedPtr<TAggregate> Aggregate)
        {
            return Async(EAsyncExecution::ThreadPool, [this, Aggregate]()
            {
                Save(Aggregate);
            });
        }
    };
}
```

---

## Capture Session Repository

```cpp
// SCM/Repositories/SCM_CaptureSessionRepository.h
#pragma once

#include "Shared/Repositories/IRepository.h"
#include "SCM_CaptureSession.h"

namespace SCM
{
    /**
     * Repository interface for CaptureSession aggregates
     */
    class ICaptureSessionRepository
        : public Shared::IRepository<FSCM_CaptureSession, FGuid>
    {
    public:
        // Domain-specific queries
        virtual TArray<TSharedPtr<FSCM_CaptureSession>> FindByState(
            ECaptureState State
        ) const = 0;

        virtual TArray<TSharedPtr<FSCM_CaptureSession>> FindByWorld(
            const UWorld* World
        ) const = 0;

        virtual TArray<TSharedPtr<FSCM_CaptureSession>> FindByDateRange(
            const FDateTime& Start,
            const FDateTime& End
        ) const = 0;

        virtual TSharedPtr<FSCM_CaptureSession> FindMostRecent() const = 0;

        // Active session management
        virtual TSharedPtr<FSCM_CaptureSession> GetActiveSession() const = 0;
        virtual void SetActiveSession(const FGuid& SessionId) = 0;
    };

    /**
     * In-memory implementation of CaptureSession repository
     */
    class FCaptureSessionMemoryRepository : public ICaptureSessionRepository
    {
    public:
        // IRepository implementation
        virtual TSharedPtr<FSCM_CaptureSession> FindById(
            const FGuid& Id
        ) const override
        {
            const TSharedPtr<FSCM_CaptureSession>* Found = Sessions.Find(Id);
            return Found ? *Found : nullptr;
        }

        virtual TArray<TSharedPtr<FSCM_CaptureSession>> FindAll() const override
        {
            TArray<TSharedPtr<FSCM_CaptureSession>> Result;
            Sessions.GenerateValueArray(Result);
            return Result;
        }

        virtual void Save(TSharedPtr<FSCM_CaptureSession> Session) override
        {
            if (Session.IsValid())
            {
                Sessions.Add(Session->GetSessionId(), Session);
            }
        }

        virtual void Remove(const FGuid& Id) override
        {
            Sessions.Remove(Id);
            if (ActiveSessionId == Id)
            {
                ActiveSessionId.Invalidate();
            }
        }

        virtual TArray<TSharedPtr<FSCM_CaptureSession>> FindBySpec(
            const Shared::TQuerySpec<FSCM_CaptureSession>& Spec
        ) const override
        {
            TArray<TSharedPtr<FSCM_CaptureSession>> Result;

            for (const auto& Pair : Sessions)
            {
                if (!Spec.Predicate || Spec.Predicate(*Pair.Value))
                {
                    Result.Add(Pair.Value);
                }
            }

            // Apply limit/offset
            if (Spec.Offset.IsSet() && Spec.Offset.GetValue() < Result.Num())
            {
                Result.RemoveAt(0, Spec.Offset.GetValue());
            }
            if (Spec.Limit.IsSet() && Spec.Limit.GetValue() < Result.Num())
            {
                Result.SetNum(Spec.Limit.GetValue());
            }

            return Result;
        }

        virtual bool Exists(const FGuid& Id) const override
        {
            return Sessions.Contains(Id);
        }

        virtual int32 Count() const override
        {
            return Sessions.Num();
        }

        // Domain-specific queries
        virtual TArray<TSharedPtr<FSCM_CaptureSession>> FindByState(
            ECaptureState State
        ) const override
        {
            Shared::TQuerySpec<FSCM_CaptureSession> Spec;
            Spec.Predicate = [State](const FSCM_CaptureSession& Session)
            {
                return Session.GetState() == State;
            };
            return FindBySpec(Spec);
        }

        virtual TArray<TSharedPtr<FSCM_CaptureSession>> FindByWorld(
            const UWorld* World
        ) const override
        {
            Shared::TQuerySpec<FSCM_CaptureSession> Spec;
            Spec.Predicate = [World](const FSCM_CaptureSession& Session)
            {
                return Session.GetWorld() == World;
            };
            return FindBySpec(Spec);
        }

        virtual TArray<TSharedPtr<FSCM_CaptureSession>> FindByDateRange(
            const FDateTime& Start,
            const FDateTime& End
        ) const override
        {
            // Implementation would filter by creation date
            return FindAll();
        }

        virtual TSharedPtr<FSCM_CaptureSession> FindMostRecent() const override
        {
            TSharedPtr<FSCM_CaptureSession> MostRecent;
            // Would sort by creation time
            if (Sessions.Num() > 0)
            {
                auto It = Sessions.CreateConstIterator();
                MostRecent = It->Value;
            }
            return MostRecent;
        }

        virtual TSharedPtr<FSCM_CaptureSession> GetActiveSession() const override
        {
            return FindById(ActiveSessionId);
        }

        virtual void SetActiveSession(const FGuid& SessionId) override
        {
            if (Exists(SessionId))
            {
                ActiveSessionId = SessionId;
            }
        }

    private:
        TMap<FGuid, TSharedPtr<FSCM_CaptureSession>> Sessions;
        FGuid ActiveSessionId;
        mutable FCriticalSection Lock;
    };
}
```

---

## Captured Frame Repository

```cpp
// SCM/Repositories/SCM_CapturedFrameRepository.h
#pragma once

#include "Shared/Repositories/IRepository.h"
#include "SCM_CapturedFrame.h"

namespace SCM
{
    /**
     * Composite key for frame identification
     */
    struct FFrameKey
    {
        FGuid SessionId;
        int32 FrameIndex;

        bool operator==(const FFrameKey& Other) const
        {
            return SessionId == Other.SessionId && FrameIndex == Other.FrameIndex;
        }

        friend uint32 GetTypeHash(const FFrameKey& Key)
        {
            return HashCombine(GetTypeHash(Key.SessionId), GetTypeHash(Key.FrameIndex));
        }
    };

    /**
     * Repository interface for CapturedFrame entities
     *
     * Note: Frames are part of CaptureSession aggregate, but this repository
     * provides optimised access for large frame collections.
     */
    class ICapturedFrameRepository
    {
    public:
        virtual ~ICapturedFrameRepository() = default;

        // Access by session and index
        virtual TSharedPtr<FSCM_CapturedFrame> FindByKey(
            const FGuid& SessionId,
            int32 FrameIndex
        ) const = 0;

        // Bulk access for a session
        virtual TArray<TSharedPtr<FSCM_CapturedFrame>> FindBySession(
            const FGuid& SessionId
        ) const = 0;

        // Range query
        virtual TArray<TSharedPtr<FSCM_CapturedFrame>> FindBySessionRange(
            const FGuid& SessionId,
            int32 StartIndex,
            int32 EndIndex
        ) const = 0;

        // Persistence
        virtual void Save(
            const FGuid& SessionId,
            TSharedPtr<FSCM_CapturedFrame> Frame
        ) = 0;

        virtual void SaveBatch(
            const FGuid& SessionId,
            const TArray<TSharedPtr<FSCM_CapturedFrame>>& Frames
        ) = 0;

        // Count
        virtual int32 CountBySession(const FGuid& SessionId) const = 0;

        // Streaming support for large datasets
        virtual void StreamFrames(
            const FGuid& SessionId,
            TFunction<void(TSharedPtr<FSCM_CapturedFrame>)> Callback
        ) const = 0;
    };

    /**
     * Disk-backed frame repository for large captures
     */
    class FCapturedFrameDiskRepository : public ICapturedFrameRepository
    {
    public:
        FCapturedFrameDiskRepository(const FString& BasePath)
            : StoragePath(BasePath)
        {}

        virtual TSharedPtr<FSCM_CapturedFrame> FindByKey(
            const FGuid& SessionId,
            int32 FrameIndex
        ) const override
        {
            FString FramePath = GetFramePath(SessionId, FrameIndex);

            if (!FPaths::FileExists(FramePath))
            {
                return nullptr;
            }

            // Load frame metadata from disk
            return LoadFrame(FramePath);
        }

        virtual TArray<TSharedPtr<FSCM_CapturedFrame>> FindBySession(
            const FGuid& SessionId
        ) const override
        {
            TArray<TSharedPtr<FSCM_CapturedFrame>> Result;

            FString SessionPath = GetSessionPath(SessionId);
            TArray<FString> FrameFiles;
            IFileManager::Get().FindFiles(
                FrameFiles,
                *(SessionPath / TEXT("*.frame")),
                true,
                false
            );

            for (const FString& File : FrameFiles)
            {
                if (auto Frame = LoadFrame(SessionPath / File))
                {
                    Result.Add(Frame);
                }
            }

            // Sort by frame index
            Result.Sort([](const auto& A, const auto& B)
            {
                return A->GetFrameIndex() < B->GetFrameIndex();
            });

            return Result;
        }

        virtual TArray<TSharedPtr<FSCM_CapturedFrame>> FindBySessionRange(
            const FGuid& SessionId,
            int32 StartIndex,
            int32 EndIndex
        ) const override
        {
            TArray<TSharedPtr<FSCM_CapturedFrame>> Result;

            for (int32 i = StartIndex; i <= EndIndex; ++i)
            {
                if (auto Frame = FindByKey(SessionId, i))
                {
                    Result.Add(Frame);
                }
            }

            return Result;
        }

        virtual void Save(
            const FGuid& SessionId,
            TSharedPtr<FSCM_CapturedFrame> Frame
        ) override
        {
            FString FramePath = GetFramePath(SessionId, Frame->GetFrameIndex());
            SaveFrame(Frame, FramePath);
        }

        virtual void SaveBatch(
            const FGuid& SessionId,
            const TArray<TSharedPtr<FSCM_CapturedFrame>>& Frames
        ) override
        {
            for (const auto& Frame : Frames)
            {
                Save(SessionId, Frame);
            }
        }

        virtual int32 CountBySession(const FGuid& SessionId) const override
        {
            FString SessionPath = GetSessionPath(SessionId);
            TArray<FString> FrameFiles;
            IFileManager::Get().FindFiles(
                FrameFiles,
                *(SessionPath / TEXT("*.frame")),
                true,
                false
            );
            return FrameFiles.Num();
        }

        virtual void StreamFrames(
            const FGuid& SessionId,
            TFunction<void(TSharedPtr<FSCM_CapturedFrame>)> Callback
        ) const override
        {
            auto Frames = FindBySession(SessionId);
            for (const auto& Frame : Frames)
            {
                Callback(Frame);
            }
        }

    private:
        FString GetSessionPath(const FGuid& SessionId) const
        {
            return StoragePath / SessionId.ToString();
        }

        FString GetFramePath(const FGuid& SessionId, int32 Index) const
        {
            return GetSessionPath(SessionId) /
                FString::Printf(TEXT("frame_%05d.frame"), Index);
        }

        TSharedPtr<FSCM_CapturedFrame> LoadFrame(const FString& Path) const;
        void SaveFrame(TSharedPtr<FSCM_CapturedFrame> Frame, const FString& Path);

        FString StoragePath;
    };
}
```

---

## Scene Data Repository

```cpp
// DEM/Repositories/DEM_SceneDataRepository.h
#pragma once

#include "Shared/Repositories/IRepository.h"
#include "DEM_SceneData.h"

namespace DEM
{
    /**
     * Repository interface for SceneData aggregates
     */
    class ISceneDataRepository
        : public Shared::IRepository<FDEM_SceneData, FGuid>
    {
    public:
        // Domain-specific queries
        virtual TSharedPtr<FDEM_SceneData> FindBySourceSession(
            const FGuid& SessionId
        ) const = 0;

        virtual TArray<TSharedPtr<FDEM_SceneData>> FindBySceneName(
            const FString& SceneName
        ) const = 0;

        // Validation
        virtual TArray<TSharedPtr<FDEM_SceneData>> FindValidated() const = 0;
        virtual TArray<TSharedPtr<FDEM_SceneData>> FindUnvalidated() const = 0;
    };

    /**
     * In-memory SceneData repository
     */
    class FSceneDataMemoryRepository : public ISceneDataRepository
    {
    public:
        virtual TSharedPtr<FDEM_SceneData> FindById(
            const FGuid& Id
        ) const override
        {
            const auto* Found = SceneDataMap.Find(Id);
            return Found ? *Found : nullptr;
        }

        virtual TArray<TSharedPtr<FDEM_SceneData>> FindAll() const override
        {
            TArray<TSharedPtr<FDEM_SceneData>> Result;
            SceneDataMap.GenerateValueArray(Result);
            return Result;
        }

        virtual void Save(TSharedPtr<FDEM_SceneData> Data) override
        {
            if (Data.IsValid())
            {
                SceneDataMap.Add(Data->GetSceneId(), Data);
                SessionToSceneMap.Add(Data->GetSourceSessionId(), Data->GetSceneId());
            }
        }

        virtual void Remove(const FGuid& Id) override
        {
            if (auto Data = FindById(Id))
            {
                SessionToSceneMap.Remove(Data->GetSourceSessionId());
            }
            SceneDataMap.Remove(Id);
        }

        virtual TArray<TSharedPtr<FDEM_SceneData>> FindBySpec(
            const Shared::TQuerySpec<FDEM_SceneData>& Spec
        ) const override
        {
            TArray<TSharedPtr<FDEM_SceneData>> Result;
            for (const auto& Pair : SceneDataMap)
            {
                if (!Spec.Predicate || Spec.Predicate(*Pair.Value))
                {
                    Result.Add(Pair.Value);
                }
            }
            return Result;
        }

        virtual bool Exists(const FGuid& Id) const override
        {
            return SceneDataMap.Contains(Id);
        }

        virtual int32 Count() const override
        {
            return SceneDataMap.Num();
        }

        // Domain queries
        virtual TSharedPtr<FDEM_SceneData> FindBySourceSession(
            const FGuid& SessionId
        ) const override
        {
            const FGuid* SceneId = SessionToSceneMap.Find(SessionId);
            return SceneId ? FindById(*SceneId) : nullptr;
        }

        virtual TArray<TSharedPtr<FDEM_SceneData>> FindBySceneName(
            const FString& SceneName
        ) const override
        {
            Shared::TQuerySpec<FDEM_SceneData> Spec;
            Spec.Predicate = [&SceneName](const FDEM_SceneData& Data)
            {
                return Data.GetMetadata().SceneName == SceneName;
            };
            return FindBySpec(Spec);
        }

        virtual TArray<TSharedPtr<FDEM_SceneData>> FindValidated() const override
        {
            Shared::TQuerySpec<FDEM_SceneData> Spec;
            Spec.Predicate = [](const FDEM_SceneData& Data)
            {
                return Data.Validate();
            };
            return FindBySpec(Spec);
        }

        virtual TArray<TSharedPtr<FDEM_SceneData>> FindUnvalidated() const override
        {
            Shared::TQuerySpec<FDEM_SceneData> Spec;
            Spec.Predicate = [](const FDEM_SceneData& Data)
            {
                return !Data.Validate();
            };
            return FindBySpec(Spec);
        }

    private:
        TMap<FGuid, TSharedPtr<FDEM_SceneData>> SceneDataMap;
        TMap<FGuid, FGuid> SessionToSceneMap;  // SessionId -> SceneId
    };
}
```

---

## Export Job Repository

```cpp
// FCM/Repositories/FCM_ExportJobRepository.h
#pragma once

#include "Shared/Repositories/IRepository.h"
#include "FCM_ExportJob.h"

namespace FCM
{
    /**
     * Repository interface for ExportJob aggregates
     */
    class IExportJobRepository
        : public Shared::IRepository<FFCM_ExportJob, FGuid>
    {
    public:
        // Query by state
        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindByState(
            EExportState State
        ) const = 0;

        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindRunning() const
        {
            return FindByState(EExportState::Running);
        }

        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindCompleted() const
        {
            return FindByState(EExportState::Completed);
        }

        // Query by format
        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindByFormat(
            EFFCM_ExportFormat Format
        ) const = 0;

        // Recent jobs
        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindRecent(
            int32 Limit = 10
        ) const = 0;
    };

    /**
     * In-memory ExportJob repository with history
     */
    class FExportJobMemoryRepository : public IExportJobRepository
    {
    public:
        virtual TSharedPtr<FFCM_ExportJob> FindById(
            const FGuid& Id
        ) const override
        {
            const auto* Found = Jobs.Find(Id);
            return Found ? *Found : nullptr;
        }

        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindAll() const override
        {
            TArray<TSharedPtr<FFCM_ExportJob>> Result;
            Jobs.GenerateValueArray(Result);
            return Result;
        }

        virtual void Save(TSharedPtr<FFCM_ExportJob> Job) override
        {
            if (Job.IsValid())
            {
                Jobs.Add(Job->GetJobId(), Job);
            }
        }

        virtual void Remove(const FGuid& Id) override
        {
            Jobs.Remove(Id);
        }

        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindBySpec(
            const Shared::TQuerySpec<FFCM_ExportJob>& Spec
        ) const override
        {
            TArray<TSharedPtr<FFCM_ExportJob>> Result;
            for (const auto& Pair : Jobs)
            {
                if (!Spec.Predicate || Spec.Predicate(*Pair.Value))
                {
                    Result.Add(Pair.Value);
                }
            }
            return Result;
        }

        virtual bool Exists(const FGuid& Id) const override
        {
            return Jobs.Contains(Id);
        }

        virtual int32 Count() const override
        {
            return Jobs.Num();
        }

        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindByState(
            EExportState State
        ) const override
        {
            Shared::TQuerySpec<FFCM_ExportJob> Spec;
            Spec.Predicate = [State](const FFCM_ExportJob& Job)
            {
                return Job.GetState() == State;
            };
            return FindBySpec(Spec);
        }

        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindByFormat(
            EFFCM_ExportFormat Format
        ) const override
        {
            Shared::TQuerySpec<FFCM_ExportJob> Spec;
            Spec.Predicate = [Format](const FFCM_ExportJob& Job)
            {
                return Job.GetConfiguration().Format == Format;
            };
            return FindBySpec(Spec);
        }

        virtual TArray<TSharedPtr<FFCM_ExportJob>> FindRecent(
            int32 Limit
        ) const override
        {
            auto All = FindAll();
            // Would sort by creation time
            if (All.Num() > Limit)
            {
                All.SetNum(Limit);
            }
            return All;
        }

    private:
        TMap<FGuid, TSharedPtr<FFCM_ExportJob>> Jobs;
    };
}
```

---

## Training Data Repository

```cpp
// TRN/Repositories/TRN_TrainingDataRepository.h
#pragma once

#include "Shared/Repositories/IRepository.h"
#include "TRN_GaussianCloud.h"
#include "TRN_TrainingSession.h"

namespace TRN
{
    /**
     * Repository interface for GaussianCloud (trained models)
     */
    class IGaussianCloudRepository
    {
    public:
        virtual ~IGaussianCloudRepository() = default;

        // Model access
        virtual TSharedPtr<FTRN_GaussianCloud> FindById(
            const FGuid& ModelId
        ) const = 0;

        virtual TArray<TSharedPtr<FTRN_GaussianCloud>> FindAll() const = 0;

        // Persistence
        virtual void Save(TSharedPtr<FTRN_GaussianCloud> Model) = 0;
        virtual void Remove(const FGuid& ModelId) = 0;

        // Query
        virtual TArray<TSharedPtr<FTRN_GaussianCloud>> FindBySourceSession(
            const FGuid& TrainingSessionId
        ) const = 0;

        // Export
        virtual FString ExportToPLY(
            const FGuid& ModelId,
            const FString& OutputPath
        ) = 0;

        virtual TSharedPtr<FTRN_GaussianCloud> ImportFromPLY(
            const FString& PLYPath
        ) = 0;
    };

    /**
     * Disk-backed repository for gaussian models
     */
    class FGaussianCloudDiskRepository : public IGaussianCloudRepository
    {
    public:
        FGaussianCloudDiskRepository(const FString& BasePath)
            : StoragePath(BasePath)
        {}

        virtual TSharedPtr<FTRN_GaussianCloud> FindById(
            const FGuid& ModelId
        ) const override
        {
            FString ModelPath = GetModelPath(ModelId);
            if (FPaths::FileExists(ModelPath))
            {
                return LoadModel(ModelPath);
            }
            return nullptr;
        }

        virtual TArray<TSharedPtr<FTRN_GaussianCloud>> FindAll() const override
        {
            TArray<TSharedPtr<FTRN_GaussianCloud>> Result;

            TArray<FString> ModelFiles;
            IFileManager::Get().FindFilesRecursive(
                ModelFiles,
                *StoragePath,
                TEXT("*.ply"),
                true,
                false
            );

            for (const FString& File : ModelFiles)
            {
                if (auto Model = LoadModel(File))
                {
                    Result.Add(Model);
                }
            }

            return Result;
        }

        virtual void Save(TSharedPtr<FTRN_GaussianCloud> Model) override
        {
            FString ModelPath = GetModelPath(Model->GetModelId());
            SaveModel(Model, ModelPath);
        }

        virtual void Remove(const FGuid& ModelId) override
        {
            FString ModelPath = GetModelPath(ModelId);
            IFileManager::Get().Delete(*ModelPath);
        }

        virtual TArray<TSharedPtr<FTRN_GaussianCloud>> FindBySourceSession(
            const FGuid& TrainingSessionId
        ) const override
        {
            // Would need metadata storage for this query
            return TArray<TSharedPtr<FTRN_GaussianCloud>>();
        }

        virtual FString ExportToPLY(
            const FGuid& ModelId,
            const FString& OutputPath
        ) override
        {
            auto Model = FindById(ModelId);
            if (Model.IsValid())
            {
                Model->ExportToPLY(OutputPath);
                return OutputPath;
            }
            return FString();
        }

        virtual TSharedPtr<FTRN_GaussianCloud> ImportFromPLY(
            const FString& PLYPath
        ) override
        {
            return FTRN_GaussianCloud::ImportFromPLY(PLYPath);
        }

    private:
        FString GetModelPath(const FGuid& ModelId) const
        {
            return StoragePath / ModelId.ToString() / TEXT("model.ply");
        }

        TSharedPtr<FTRN_GaussianCloud> LoadModel(const FString& Path) const;
        void SaveModel(TSharedPtr<FTRN_GaussianCloud> Model, const FString& Path);

        FString StoragePath;
    };
}
```

---

## Unit of Work Pattern

```cpp
// Shared/Repositories/UnitOfWork.h
#pragma once

#include "CoreMinimal.h"

namespace Shared
{
    /**
     * Unit of Work pattern for coordinating repository transactions
     */
    class FUnitOfWork
    {
    public:
        void RegisterNew(TSharedPtr<void> Entity, const FString& RepositoryType);
        void RegisterDirty(TSharedPtr<void> Entity, const FString& RepositoryType);
        void RegisterDeleted(TSharedPtr<void> Entity, const FString& RepositoryType);

        void Commit();
        void Rollback();

    private:
        TArray<TPair<TSharedPtr<void>, FString>> NewEntities;
        TArray<TPair<TSharedPtr<void>, FString>> DirtyEntities;
        TArray<TPair<TSharedPtr<void>, FString>> DeletedEntities;
    };
}
```
