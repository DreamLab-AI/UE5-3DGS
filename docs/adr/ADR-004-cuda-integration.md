# ADR-004: Optional CUDA Integration for Training Module

## Status
Accepted

## Context

In-engine 3DGS training requires GPU compute capabilities:

| Component | GPU Requirement | CUDA Features Used |
|-----------|-----------------|-------------------|
| Gaussian Rasterizer | Required | Custom kernels, shared memory |
| Adam Optimizer | Required | Tensor operations |
| Densification | Required | Parallel reduction |
| SH Computation | Required | SIMD operations |

Key considerations:

1. **UE5 GPU Context**: UE5 manages its own D3D12/Vulkan context; CUDA interop is complex
2. **CUDA Version Conflicts**: UE5 may use different CUDA versions than 3DGS reference
3. **Platform Support**: CUDA only available on NVIDIA GPUs (Windows/Linux)
4. **User Base**: Many users want export-only without GPU compute dependencies
5. **Phase Separation**: PRD designates in-engine training as Phase 2

The PRD identifies "CUDA version conflicts with UE5 dependencies" as a medium-probability, medium-impact risk (Section 9.1).

## Decision

We will implement CUDA integration as an **optional, isolated module** with the following design:

### Module Isolation

```
UE5_3DGS_Plugin/
├── Source/
│   ├── UE5_3DGS/
│   │   ├── SCM/    # No CUDA dependency
│   │   ├── DEM/    # No CUDA dependency
│   │   ├── FCM/    # No CUDA dependency
│   │   └── TRN/    # CUDA dependency (optional)
│   │       ├── TRN_CUDAIntegration.h
│   │       ├── TRN_CUDAIntegration.cpp
│   │       └── TRN_Kernels.cu
└── ThirdParty/
    └── CUDA/       # Only included when WITH_CUDA=1
```

### Conditional Compilation

```csharp
// UE5_3DGS.Build.cs
public class UE5_3DGS : ModuleRules
{
    public UE5_3DGS(ReadOnlyTargetRules Target) : base(Target)
    {
        // CUDA integration (optional)
        bool bEnableCUDA = false;

        if (Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.Linux)
        {
            string CUDAPath = Environment.GetEnvironmentVariable("CUDA_PATH");
            if (!string.IsNullOrEmpty(CUDAPath) &&
                File.Exists(Path.Combine(CUDAPath, "include", "cuda.h")))
            {
                bEnableCUDA = true;

                PublicIncludePaths.Add(Path.Combine(CUDAPath, "include"));
                PublicAdditionalLibraries.Add(
                    Path.Combine(CUDAPath, "lib", "x64", "cuda.lib")
                );
                PublicAdditionalLibraries.Add(
                    Path.Combine(CUDAPath, "lib", "x64", "cudart.lib")
                );

                PublicDefinitions.Add("WITH_CUDA=1");
            }
        }

        if (!bEnableCUDA)
        {
            PublicDefinitions.Add("WITH_CUDA=0");
        }
    }
}
```

### Interface Abstraction

```cpp
// TRN_TrainingInterface.h - Abstract interface (no CUDA types)
class ITRN_TrainingModule
{
public:
    virtual ~ITRN_TrainingModule() = default;

    virtual bool IsAvailable() const = 0;
    virtual bool Initialize(const FTRN_Config& Config) = 0;
    virtual void StartTraining(const FString& COLMAPPath) = 0;
    virtual void StopTraining() = 0;
    virtual FTRN_Metrics GetMetrics() const = 0;
    virtual void ExportPLY(const FString& OutputPath) = 0;
};

// Factory function
TSharedPtr<ITRN_TrainingModule> CreateTrainingModule();
```

### Stub Implementation (No CUDA)

```cpp
// TRN_TrainingStub.cpp - Compiled when WITH_CUDA=0
#if !WITH_CUDA

class FTRN_TrainingStub : public ITRN_TrainingModule
{
public:
    bool IsAvailable() const override { return false; }

    bool Initialize(const FTRN_Config& Config) override
    {
        UE_LOG(LogUE5_3DGS, Warning,
            TEXT("Training module not available: CUDA not enabled at build time"));
        return false;
    }

    // Other methods return graceful failures
};

TSharedPtr<ITRN_TrainingModule> CreateTrainingModule()
{
    return MakeShared<FTRN_TrainingStub>();
}

#endif
```

### CUDA Implementation

```cpp
// TRN_TrainingCUDA.cpp - Compiled when WITH_CUDA=1
#if WITH_CUDA

#include <cuda_runtime.h>

class FTRN_TrainingCUDA : public ITRN_TrainingModule
{
public:
    bool IsAvailable() const override
    {
        int DeviceCount = 0;
        cudaError_t Result = cudaGetDeviceCount(&DeviceCount);
        return (Result == cudaSuccess && DeviceCount > 0);
    }

    bool Initialize(const FTRN_Config& Config) override
    {
        // Create CUDA context separate from UE5's GPU context
        cudaError_t Result = cudaSetDevice(Config.GPUIndex);
        if (Result != cudaSuccess)
        {
            UE_LOG(LogUE5_3DGS, Error,
                TEXT("Failed to initialise CUDA: %s"),
                UTF8_TO_TCHAR(cudaGetErrorString(Result)));
            return false;
        }

        // Allocate GPU memory for Gaussian parameters
        // ...

        return true;
    }

    // Full training implementation
};

TSharedPtr<ITRN_TrainingModule> CreateTrainingModule()
{
    return MakeShared<FTRN_TrainingCUDA>();
}

#endif
```

### Runtime Detection

```cpp
// Check availability at runtime
void UUE5_3DGS_Subsystem::Initialize()
{
    TrainingModule = CreateTrainingModule();

    if (TrainingModule->IsAvailable())
    {
        UE_LOG(LogUE5_3DGS, Log,
            TEXT("CUDA training module available"));
    }
    else
    {
        UE_LOG(LogUE5_3DGS, Log,
            TEXT("CUDA training not available - export-only mode"));
    }
}
```

## Consequences

### Positive

- **Export Always Works**: Core SCM/DEM/FCM modules have no CUDA dependency
- **Graceful Degradation**: UI detects and disables training features when unavailable
- **Build Flexibility**: Plugin compiles successfully without CUDA SDK installed
- **Platform Independence**: macOS/ARM builds work (export-only)
- **Reduced Attack Surface**: No GPU driver dependencies for export use cases
- **Phase 2 Ready**: Training module can be developed independently

### Negative

- **Feature Disparity**: Training features unavailable to some users
- **Build Complexity**: Conditional compilation requires careful testing
- **Duplicate Code Paths**: Stub and real implementations must stay synchronised
- **Documentation**: Must clearly communicate feature availability

### Hardware Requirements

| Feature Set | GPU Required | CUDA Required |
|-------------|--------------|---------------|
| Export Only | Any (for UE5 rendering) | No |
| With Training | NVIDIA 8GB+ VRAM | CUDA 11.8+ |

## Alternatives Considered

### 1. Mandatory CUDA Dependency

**Description**: Require CUDA for all builds

**Rejected Because**:
- Excludes users who only need export
- Complicates CI/CD (needs CUDA runners)
- Reduces potential user base
- macOS users completely excluded

### 2. External Training Process

**Description**: Export only; users run separate Python training script

**Adopted as Primary Workflow**: This is the Phase 1 approach

**In-engine training is Phase 2 enhancement**

### 3. Vulkan/D3D12 Compute Shaders

**Description**: Implement training using UE5's native GPU compute

**Rejected Because**:
- Significant engineering effort
- Performance would be inferior to CUDA
- Reference implementations use CUDA
- No community code to leverage

### 4. DirectML/ROCm Support

**Description**: Support AMD GPUs via DirectML or ROCm

**Deferred to Future**: May consider if demand exists, but NVIDIA dominates ML workloads

## References

- PRD Section 4.3: Optional In-Engine Training Module
- PRD Section 5.2: Build Configuration (CUDA conditional)
- PRD Section 9.1: Technical Risks (CUDA version conflicts)
- CUDA-UE5 Interop: https://developer.nvidia.com/cuda-interoperability
- Research: Training Techniques (`docs/research/training-techniques.md`)
