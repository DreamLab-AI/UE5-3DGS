# ADR-001: UE5 Plugin Architecture

## Status
Accepted

## Context

UE5-3DGS requires a robust plugin architecture that:

1. Integrates seamlessly with Unreal Engine 5.3+ editor and runtime
2. Supports modular development with clear separation of concerns
3. Enables optional components (e.g., CUDA training) without bloating core functionality
4. Allows for parallel development by multiple agents/developers
5. Facilitates testing and maintenance of individual components

The plugin must handle four distinct functional areas:
- Scene capture and camera trajectory generation
- Data extraction from UE5 render buffers
- Format conversion and export
- Optional in-engine training (Phase 2)

## Decision

We will implement a **modular plugin architecture** with four primary modules:

### Module Structure

```
UE5_3DGS_Plugin/
├── Source/
│   ├── UE5_3DGS/                    # Runtime module
│   │   ├── Public/
│   │   │   ├── SCM/                 # Scene Capture Module
│   │   │   ├── DEM/                 # Data Extraction Module
│   │   │   ├── FCM/                 # Format Conversion Module
│   │   │   ├── TRN/                 # Training Module (Optional)
│   │   │   └── UI/                  # User Interface
│   │   └── Private/
│   └── UE5_3DGS_Editor/             # Editor-only module
├── Shaders/
├── Content/
├── ThirdParty/
└── Config/
```

### Module Responsibilities

| Module | Abbreviation | Responsibility | Dependencies |
|--------|--------------|----------------|--------------|
| Scene Capture Module | SCM | Camera trajectories, frame capture, coverage analysis | UE5 Core |
| Data Extraction Module | DEM | Buffer extraction, coordinate conversion, camera parameters | SCM |
| Format Conversion Module | FCM | COLMAP export, PLY export, manifest generation | DEM |
| Training Module | TRN | CUDA integration, optimizer, rasterizer | FCM, CUDA (optional) |

### Build Configuration

```csharp
// UE5_3DGS.Build.cs
public class UE5_3DGS : ModuleRules
{
    public UE5_3DGS(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Core dependencies (always included)
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine",
            "RenderCore", "RHI", "Renderer"
        });

        // Optional CUDA (conditional compilation)
        if (HasCUDASupport(Target)) {
            PublicDefinitions.Add("WITH_CUDA=1");
            // Add CUDA libraries
        }
    }
}
```

### Interface Design

Each module exposes well-defined interfaces:

```cpp
// SCM Interface
class ISCM_TrajectoryGenerator {
    virtual TArray<FTransform> GenerateTrajectory(
        const FSCM_TrajectoryParams& Params) = 0;
};

// DEM Interface
class IDEM_BufferExtractor {
    virtual TSharedPtr<FImage> ExtractColorBuffer(
        UTextureRenderTarget2D* Target) = 0;
    virtual TSharedPtr<FFloatImage> ExtractDepthBuffer(
        UTextureRenderTarget2D* Target) = 0;
};

// FCM Interface
class IFCM_FormatWriter {
    virtual bool WriteDataset(
        const FString& OutputPath,
        const FFCM_ExportData& Data) = 0;
};
```

## Consequences

### Positive

- **Separation of Concerns**: Each module can be developed, tested, and maintained independently
- **Optional Features**: TRN module can be excluded entirely for export-only builds
- **Parallel Development**: Multiple agents can work on different modules simultaneously
- **Testability**: Unit tests can target specific modules without full plugin integration
- **Extensibility**: New export formats or trajectory types can be added without modifying core modules
- **Reduced Build Times**: Conditional compilation excludes unused features

### Negative

- **Interface Overhead**: Well-defined interfaces require additional abstraction layers
- **Complexity**: Module boundaries must be carefully designed and maintained
- **Documentation**: Each module requires separate documentation
- **Inter-Module Communication**: Data passing between modules needs careful design

### Risks Mitigated

- **UE5 Version Changes**: Abstraction layers isolate UE5-specific code
- **CUDA Conflicts**: Optional TRN module prevents CUDA dependency issues
- **Scope Creep**: Clear module boundaries prevent feature bloat

## Alternatives Considered

### 1. Monolithic Plugin

**Description**: Single module containing all functionality

**Rejected Because**:
- Increases build times
- Makes testing difficult
- Creates tight coupling
- Cannot exclude optional features

### 2. Separate Plugins

**Description**: Each module as independent plugin (UE5_3DGS_Capture, UE5_3DGS_Export, etc.)

**Rejected Because**:
- Complicates installation
- Plugin interdependencies are harder to manage
- User confusion with multiple plugins

### 3. Blueprint-Only Architecture

**Description**: Implement entirely in Blueprint/Niagara

**Rejected Because**:
- Performance limitations for buffer extraction
- No CUDA access
- Harder to maintain and version control
- Limited access to low-level render APIs

## References

- PRD Section 5.1: Plugin Structure
- UE5 Plugin Documentation: https://docs.unrealengine.com/5.0/en-US/plugins-in-unreal-engine/
- Research: Format Standards (`docs/research/format-standards.md`)
