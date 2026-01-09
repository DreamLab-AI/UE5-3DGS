# SPARC Specification Document
## UE5-3DGS: In-Engine 3D Gaussian Splatting Pipeline

**Version:** 1.0
**Phase:** Specification
**Status:** Draft

---

## 1. Module Specifications

### 1.1 Scene Capture Module (SCM)

#### 1.1.1 Overview
The Scene Capture Module orchestrates camera trajectory generation and high-fidelity frame rendering for 3DGS dataset creation.

#### 1.1.2 Functional Requirements

| ID | Requirement | Priority | Acceptance Criteria |
|----|-------------|----------|---------------------|
| SCM-FR-001 | Generate spherical camera trajectories using Fibonacci lattice | P0 | Generates N uniformly distributed camera positions on sphere surface with angular deviation < 5% from theoretical uniform |
| SCM-FR-002 | Support orbital trajectory around scene centroid | P0 | Orbit maintains constant radius with configurable elevation angle (0-90 degrees) |
| SCM-FR-003 | Support spline-based flythrough paths | P1 | Accept user-defined spline with Catmull-Rom interpolation at configurable sample density |
| SCM-FR-004 | Execute high-fidelity rendering at configurable resolutions | P0 | Support 720p, 1080p, 2K, 4K presets and custom resolution up to 8K |
| SCM-FR-005 | Extract per-frame metadata with nanosecond precision timestamps | P0 | Timestamps accurate to 1 microsecond, monotonically increasing |
| SCM-FR-006 | Support asynchronous batch capture mode | P0 | Non-blocking capture with progress callback at < 100ms latency |
| SCM-FR-007 | Provide coverage analysis visualization | P1 | Display scene coverage heatmap in editor viewport |
| SCM-FR-008 | Support pause/resume capture sessions | P1 | Resume from exact frame index with no duplicate/missing frames |

#### 1.1.3 Non-Functional Requirements

| ID | Requirement | Target |
|----|-------------|--------|
| SCM-NFR-001 | Capture throughput | >= 5 fps at 1080p on reference hardware (RTX 3080) |
| SCM-NFR-002 | Memory footprint per frame | < 100MB for 1080p RGBA + Depth + Normal |
| SCM-NFR-003 | Startup latency | < 2 seconds from capture initiation to first frame |
| SCM-NFR-004 | CPU utilization during capture | < 50% single core for orchestration |

#### 1.1.4 Acceptance Criteria

**AC-SCM-001: Spherical Trajectory Generation**
```
GIVEN a target scene with known bounding box
AND a request for N camera positions with radius R
WHEN spherical trajectory is generated
THEN N positions are created on sphere surface
AND all positions face toward scene centroid
AND angular distribution variance is within 5% of Fibonacci lattice theoretical
AND no two cameras are closer than 2*pi*R/sqrt(N) * 0.8
```

**AC-SCM-002: Frame Capture Integrity**
```
GIVEN a capture session with N frames requested
WHEN capture completes successfully
THEN exactly N frames are captured
AND each frame has valid RGB buffer (no null pixels)
AND each frame has valid depth buffer (all values in valid range)
AND frame indices are sequential 0 to N-1
AND timestamps are monotonically increasing
```

**AC-SCM-003: Async Capture Progress**
```
GIVEN an async capture session
WHEN capture is in progress
THEN progress callback fires at least once per frame
AND callback latency from frame completion to callback is < 100ms
AND cancellation request stops capture within 500ms
```

---

### 1.2 Data Extraction Module (DEM)

#### 1.2.1 Overview
The Data Extraction Module handles buffer extraction from render targets and coordinate system conversion between UE5 and target formats.

#### 1.2.2 Functional Requirements

| ID | Requirement | Priority | Acceptance Criteria |
|----|-------------|----------|---------------------|
| DEM-FR-001 | Extract RGB color buffer in sRGB color space | P0 | Output PNG/EXR with correct gamma encoding |
| DEM-FR-002 | Extract depth buffer with reverse-Z to linear conversion | P0 | Output linear depth in metres with < 0.1% error at all ranges |
| DEM-FR-003 | Extract world-space normal buffer | P1 | Normal vectors normalized, world-space aligned |
| DEM-FR-004 | Convert UE5 transforms to COLMAP convention | P0 | Round-trip conversion error < 1mm position, < 0.01 degree rotation |
| DEM-FR-005 | Convert UE5 transforms to OpenCV convention | P1 | Support OpenCV camera model output |
| DEM-FR-006 | Generate camera intrinsic matrices | P0 | fx, fy, cx, cy accurate to < 0.5 pixel error |
| DEM-FR-007 | Support multiple camera models (PINHOLE, OPENCV) | P1 | Distortion coefficients exported when applicable |
| DEM-FR-008 | Compute scene bounds in target coordinate system | P0 | Bounds enclose all visible geometry |

#### 1.2.3 Coordinate System Specifications

**UE5 Native (Source):**
- Left-handed coordinate system
- Z-up, X-forward, Y-right
- Units: centimetres
- Rotations: FQuat (X, Y, Z, W order)

**COLMAP Target:**
- Right-handed coordinate system
- Y-down, Z-forward (camera looking along +Z)
- Units: metres (configurable)
- Rotations: Quaternion (W, X, Y, Z order)

**Conversion Matrix:**
```
COLMAP_from_UE5 = [
    [0,  1,  0, 0],
    [0,  0, -1, 0],
    [1,  0,  0, 0],
    [0,  0,  0, 1]
] * scale(0.01)  // cm to metres
```

#### 1.2.4 Acceptance Criteria

**AC-DEM-001: Coordinate Round-Trip**
```
GIVEN a camera transform T_ue5 in UE5 coordinates
WHEN converted to COLMAP convention and back
THEN position error < 1mm (0.1 UE units)
AND rotation error < 0.01 degrees (measured as quaternion angle)
AND the operation is deterministic (same input = same output)
```

**AC-DEM-002: Depth Buffer Accuracy**
```
GIVEN a scene with objects at known distances [1m, 10m, 100m, 1000m]
WHEN depth buffer is extracted and converted to linear
THEN depth values match known distances within 0.1% relative error
AND no invalid values (NaN, Inf) in visible pixel regions
AND depth increases monotonically along rays (no Z-fighting artifacts)
```

**AC-DEM-003: Intrinsic Matrix Accuracy**
```
GIVEN a camera with known FOV and resolution
WHEN intrinsic matrix K is computed
THEN reprojection of 3D points at scene corners
AND comparison with actual rendered pixel positions
AND error < 0.5 pixels at image edges
```

---

### 1.3 Format Conversion Module (FCM)

#### 1.3.1 Overview
The Format Conversion Module exports captured data to COLMAP and PLY formats compatible with 3DGS training pipelines.

#### 1.3.2 Functional Requirements

| ID | Requirement | Priority | Acceptance Criteria |
|----|-------------|----------|---------------------|
| FCM-FR-001 | Write COLMAP cameras.txt with PINHOLE model | P0 | Parseable by COLMAP CLI |
| FCM-FR-002 | Write COLMAP cameras.bin in binary format | P0 | Binary identical semantics to text format |
| FCM-FR-003 | Write COLMAP images.txt with quaternion + translation | P0 | Parseable by COLMAP CLI |
| FCM-FR-004 | Write COLMAP images.bin in binary format | P0 | Binary identical semantics to text format |
| FCM-FR-005 | Write COLMAP points3D.txt (empty or sparse) | P0 | Valid empty file or optional SfM points |
| FCM-FR-006 | Generate training manifest JSON | P1 | Schema-compliant JSON with all metadata |
| FCM-FR-007 | Write PLY file with gaussian splat header | P2 | Compatible with reference 3DGS implementation |
| FCM-FR-008 | Support configurable output directory structure | P0 | Create images/, sparse/0/ subdirectories |

#### 1.3.3 COLMAP Format Specification

**cameras.txt format:**
```
# Camera list with one line of data per camera:
# CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]
# For PINHOLE: PARAMS = fx, fy, cx, cy
1 PINHOLE 1920 1080 1597.23 1597.23 960.0 540.0
```

**images.txt format:**
```
# Image list with two lines of data per image:
# IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME
# POINTS2D[] as (X, Y, POINT3D_ID)
1 0.9659 0.0 0.2588 0.0 1.5 -2.0 3.0 1 frame_00000.png

```

#### 1.3.4 Acceptance Criteria

**AC-FCM-001: COLMAP CLI Acceptance**
```
GIVEN exported COLMAP dataset
WHEN processed with `colmap model_analyzer --input_path sparse/0`
THEN command exits with code 0
AND reports correct number of cameras and images
AND no parsing errors in output
```

**AC-FCM-002: 3DGS Training Compatibility**
```
GIVEN exported COLMAP dataset
WHEN used as input to gaussian-splatting train.py
THEN training initiates without data loading errors
AND first iteration completes successfully
AND loss is finite and decreasing
```

**AC-FCM-003: Binary/Text Equivalence**
```
GIVEN same capture data
WHEN exported to both text and binary COLMAP formats
AND binary files converted to text using COLMAP model_converter
THEN text outputs are semantically identical
AND numerical values differ by < 1e-6
```

---

### 1.4 Training Module (TRN) - Phase 2

#### 1.4.1 Overview
Optional in-engine 3DGS training using CUDA integration.

#### 1.4.2 Functional Requirements

| ID | Requirement | Priority | Acceptance Criteria |
|----|-------------|----------|---------------------|
| TRN-FR-001 | Initialize CUDA context within UE5 | P2 | CUDA operations execute without driver errors |
| TRN-FR-002 | Load training data from COLMAP export | P2 | Parse cameras, images, and optional points |
| TRN-FR-003 | Implement Adam optimizer for gaussian parameters | P2 | Convergence matches reference implementation within 5% |
| TRN-FR-004 | Implement adaptive densification | P2 | Gaussian count increases appropriately during training |
| TRN-FR-005 | Implement tile-based rasterizer | P2 | Render speed > 100 fps at 1080p for 1M gaussians |
| TRN-FR-006 | Export trained model to PLY format | P2 | PLY loadable by reference viewers |
| TRN-FR-007 | Provide real-time training visualization | P2 | Preview render updates at > 10 fps during training |

#### 1.4.3 Acceptance Criteria

**AC-TRN-001: Training Convergence**
```
GIVEN reference test scene with known optimal PSNR
WHEN trained for 30,000 iterations with default parameters
THEN final PSNR >= 30dB on holdout views
AND SSIM >= 0.95 on holdout views
AND training completes within 30 minutes on RTX 3080
```

---

## 2. Interface Specifications

### 2.1 C++ API Surface

```cpp
// Main orchestrator interface
class IUE5_3DGS_CaptureOrchestrator
{
public:
    virtual ~IUE5_3DGS_CaptureOrchestrator() = default;

    // Configuration
    virtual void SetCaptureResolution(FIntPoint Resolution) = 0;
    virtual void SetTrajectoryType(ESCM_TrajectoryType Type) = 0;
    virtual void SetTrajectoryParameters(const FTrajectoryParams& Params) = 0;
    virtual void SetOutputPath(const FString& Path) = 0;
    virtual void SetExportFormat(EFCM_ExportFormat Format) = 0;

    // Execution
    virtual TFuture<FCaptureResult> ExecuteCaptureAsync(UWorld* World) = 0;
    virtual void PauseCapture() = 0;
    virtual void ResumeCapture() = 0;
    virtual void CancelCapture() = 0;

    // Progress
    virtual FOnCaptureProgress& OnProgress() = 0;
    virtual float GetProgressPercentage() const = 0;
    virtual int32 GetCapturedFrameCount() const = 0;
};

// Coordinate conversion interface
class IUE5_3DGS_CoordinateConverter
{
public:
    virtual FMatrix ConvertTransformToColmap(const FTransform& UE5Transform) = 0;
    virtual FTransform ConvertTransformFromColmap(const FMatrix& ColmapPose) = 0;
    virtual FVector ConvertPositionToColmap(const FVector& UE5Position) = 0;
    virtual FQuat ConvertRotationToColmap(const FQuat& UE5Rotation) = 0;
};

// Export interface
class IUE5_3DGS_Exporter
{
public:
    virtual bool ExportToCOLMAP(
        const FString& OutputPath,
        const TArray<FCapturedFrame>& Frames,
        bool bBinary = true
    ) = 0;

    virtual bool ExportToPLY(
        const FString& OutputPath,
        const TArray<F3DGS_Splat>& Splats
    ) = 0;

    virtual bool WriteManifest(
        const FString& OutputPath,
        const FCaptureManifest& Manifest
    ) = 0;
};
```

### 2.2 Blueprint API Surface

```cpp
UFUNCTION(BlueprintCallable, Category = "3DGS")
static void StartCapture(
    UWorld* World,
    ESCM_TrajectoryType Trajectory,
    int32 NumFrames,
    FIntPoint Resolution,
    const FString& OutputPath
);

UFUNCTION(BlueprintCallable, Category = "3DGS")
static void StopCapture();

UFUNCTION(BlueprintPure, Category = "3DGS")
static float GetCaptureProgress();

UFUNCTION(BlueprintCallable, Category = "3DGS")
static bool ExportToColmap(const FString& OutputPath);
```

---

## 3. Data Specifications

### 3.1 Captured Frame Structure

```cpp
struct FCapturedFrame
{
    // Identification
    int32 FrameIndex;
    double TimestampSeconds;
    FString ImageFilename;

    // Camera extrinsics (UE5 world space)
    FTransform CameraTransform;

    // Camera intrinsics
    float FocalLengthPixels_X;
    float FocalLengthPixels_Y;
    float PrincipalPoint_X;
    float PrincipalPoint_Y;
    int32 ImageWidth;
    int32 ImageHeight;
    float FieldOfView_Degrees;
    float NearClipPlane;
    float FarClipPlane;

    // Buffer data (compressed)
    TArray<uint8> ColorBuffer_PNG;
    TArray<float> DepthBuffer_Linear;
    TArray<FVector> NormalBuffer_WorldSpace;

    // Metadata
    FBox SceneBoundsWorld;
    TArray<FString> VisibleActorNames;
};
```

### 3.2 Export Configuration

```cpp
struct FExportConfiguration
{
    // Output format
    EFCM_ExportFormat Format = EFCM_ExportFormat::COLMAP_Binary;

    // Coordinate convention
    EDEM_CoordinateConvention Convention = EDEM_CoordinateConvention::COLMAP_RightHanded;

    // Scale factor (UE5 units to target units)
    float ScaleFactor = 0.01f;  // cm to metres

    // Image format
    EFCM_ImageFormat ImageFormat = EFCM_ImageFormat::PNG;
    int32 JPEGQuality = 95;

    // Optional outputs
    bool bExportDepthMaps = false;
    bool bExportNormalMaps = false;
    bool bWriteManifest = true;

    // Camera model
    EFCM_CameraModel CameraModel = EFCM_CameraModel::PINHOLE;
};
```

---

## 4. Error Handling Specifications

### 4.1 Error Codes

| Code | Name | Description | Recovery |
|------|------|-------------|----------|
| E_SCM_001 | INVALID_WORLD | Null or invalid UWorld pointer | Check world validity before capture |
| E_SCM_002 | TRAJECTORY_FAILED | Trajectory generation failed | Verify scene bounds, reduce camera count |
| E_SCM_003 | RENDER_FAILED | Frame rendering failed | Check GPU memory, reduce resolution |
| E_DEM_001 | BUFFER_EXTRACTION_FAILED | Could not read render target | Verify render target is valid |
| E_DEM_002 | COORDINATE_OVERFLOW | Position exceeds float precision | Scale scene or use origin rebasing |
| E_FCM_001 | FILE_WRITE_FAILED | Could not write output file | Check permissions, disk space |
| E_FCM_002 | INVALID_FORMAT | Unsupported export format | Use supported format |

### 4.2 Validation Points

1. **Pre-capture validation:**
   - World pointer validity
   - Scene bounds computation
   - Output path writability
   - Memory availability estimate

2. **Per-frame validation:**
   - Buffer non-null check
   - Depth range sanity
   - Transform validity (no NaN/Inf)

3. **Post-capture validation:**
   - Frame count matches request
   - All files written successfully
   - COLMAP format parseable

---

## 5. Performance Specifications

### 5.1 Benchmarks

| Operation | Target | Hardware Reference |
|-----------|--------|-------------------|
| Trajectory generation (1000 cameras) | < 100ms | Intel i7-10700K |
| Single frame capture (1080p) | < 200ms | RTX 3080 |
| Single frame capture (4K) | < 500ms | RTX 3080 |
| Depth buffer extraction (1080p) | < 10ms | Any |
| Coordinate conversion (single) | < 1us | Any |
| COLMAP export (1000 images) | < 5s | NVMe SSD |

### 5.2 Memory Limits

| Resource | Limit | Notes |
|----------|-------|-------|
| Render target pool | 4GB | Configurable via settings |
| Frame buffer queue | 10 frames | Async pipeline depth |
| Export buffer | 1GB | Streaming write |

---

## 6. Traceability Matrix

| Requirement | Test Case | Implementation Module |
|-------------|-----------|----------------------|
| SCM-FR-001 | TC-SCM-001, TC-SCM-002 | SCM_SphericalTrajectory |
| SCM-FR-002 | TC-SCM-003 | SCM_OrbitalTrajectory |
| SCM-FR-003 | TC-SCM-004 | SCM_SplineTrajectory |
| DEM-FR-001 | TC-DEM-001, TC-DEM-002 | DEM_ColorExtractor |
| DEM-FR-002 | TC-DEM-003, TC-DEM-004 | DEM_DepthExtractor |
| DEM-FR-004 | TC-DEM-005, TC-DEM-006 | DEM_CoordinateConverter |
| FCM-FR-001 | TC-FCM-001 | FCM_COLMAPTextWriter |
| FCM-FR-002 | TC-FCM-002 | FCM_COLMAPBinaryWriter |
| FCM-FR-003 | TC-FCM-003 | FCM_COLMAPTextWriter |

---

*Document Version: 1.0*
*Last Updated: January 2025*
