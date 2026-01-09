# ADR-003: COLMAP as Primary Export Format

## Status
Accepted

## Context

3D Gaussian Splatting training pipelines accept input data in various formats. The primary options are:

| Format | Tool Ecosystem | Adoption | Features |
|--------|----------------|----------|----------|
| COLMAP | graphdeco-inria, gsplat, nerfstudio | De-facto standard | Cameras, images, sparse points |
| NeRF Synthetic | instant-ngp, NeRFStudio | Common for synthetic | JSON transforms, images |
| Polycam | Polycam exports | Proprietary | Cameras, images |
| Custom | Various | Fragmented | Varies |

The canonical 3DGS implementation from Inria (`graphdeco-inria/gaussian-splatting`) expects COLMAP format:

```
input/
├── images/
│   ├── frame_00000.png
│   └── ...
└── sparse/
    └── 0/
        ├── cameras.bin
        ├── images.bin
        └── points3D.bin
```

Key considerations:
1. **Research Adoption**: Most 3DGS research uses COLMAP format
2. **Tool Support**: COLMAP CLI validates exports
3. **Feature Completeness**: Supports all required camera models
4. **Ground Truth**: UE5 provides perfect camera poses (no SfM estimation needed)

## Decision

We will use **COLMAP format as the primary export target**, with both text and binary variants supported.

### Format Structure

```
UE5_3DGS_Export/
├── images/
│   ├── frame_00000.png
│   ├── frame_00001.png
│   └── ...
├── depth_maps/              # Optional ground-truth depth
│   ├── frame_00000.exr
│   └── ...
├── sparse/
│   └── 0/
│       ├── cameras.bin      # Camera intrinsics
│       ├── images.bin       # Camera extrinsics + image names
│       └── points3D.bin     # Can be empty (3DGS initialises from images)
├── database.db              # Optional COLMAP database
└── manifest.json            # UE5-3DGS custom metadata
```

### Camera Model Selection

```cpp
enum class EFCM_CameraModel : int32
{
    SIMPLE_PINHOLE = 0,  // fx, cx, cy (square pixels)
    PINHOLE = 1,         // fx, fy, cx, cy (default)
    SIMPLE_RADIAL = 2,   // fx, cx, cy, k1
    RADIAL = 3,          // fx, cx, cy, k1, k2
    OPENCV = 4,          // fx, fy, cx, cy, k1, k2, p1, p2
    OPENCV_FISHEYE = 5   // fx, fy, cx, cy, k1, k2, k3, k4
};

// Default: PINHOLE (no distortion from virtual cameras)
static constexpr EFCM_CameraModel DefaultModel = EFCM_CameraModel::PINHOLE;
```

### Binary Format Implementation

```cpp
class FFCM_COLMAPBinaryWriter
{
public:
    void WriteCamerasFile(const FString& Path, const TArray<FDEM_CameraParameters>& Cameras)
    {
        FArchive& Ar = *IFileManager::Get().CreateFileWriter(*Path);

        uint64 NumCameras = Cameras.Num();
        Ar << NumCameras;

        for (const auto& Camera : Cameras)
        {
            uint32 CameraId = Camera.Id;
            int32 ModelId = static_cast<int32>(Camera.Model);
            uint64 Width = Camera.ImageWidth;
            uint64 Height = Camera.ImageHeight;

            Ar << CameraId << ModelId << Width << Height;

            // PINHOLE params: fx, fy, cx, cy
            Ar << Camera.fx << Camera.fy << Camera.cx << Camera.cy;
        }
    }

    void WriteImagesFile(const FString& Path,
                         const TArray<FDEM_CameraParameters>& Cameras,
                         const TArray<FString>& ImageNames)
    {
        FArchive& Ar = *IFileManager::Get().CreateFileWriter(*Path);

        uint64 NumImages = Cameras.Num();
        Ar << NumImages;

        for (int32 i = 0; i < Cameras.Num(); ++i)
        {
            uint32 ImageId = i + 1;

            // Quaternion (WXYZ order, world-to-camera)
            double qw = Cameras[i].q.W;
            double qx = Cameras[i].q.X;
            double qy = Cameras[i].q.Y;
            double qz = Cameras[i].q.Z;

            // Translation (world-to-camera)
            double tx = Cameras[i].t.X;
            double ty = Cameras[i].t.Y;
            double tz = Cameras[i].t.Z;

            uint32 CameraId = Cameras[i].Id;

            Ar << ImageId << qw << qx << qy << qz << tx << ty << tz << CameraId;

            // Image name (null-terminated string)
            FString Name = ImageNames[i];
            Ar.Serialize(TCHAR_TO_UTF8(*Name), Name.Len() + 1);

            // Points2D (empty for synthetic data)
            uint64 NumPoints2D = 0;
            Ar << NumPoints2D;
        }
    }
};
```

### Why Not NeRF Synthetic Format?

| Aspect | COLMAP | NeRF Synthetic |
|--------|--------|----------------|
| 3DGS Compatibility | Native | Requires conversion |
| Camera Models | Full range | Only perspective |
| Validation Tool | COLMAP CLI | None |
| Research Standard | Yes | Declining |
| Depth Support | Via dense model | JSON extension |

## Consequences

### Positive

- **Maximum Compatibility**: Works with canonical 3DGS, gsplat, nerfstudio, and all major implementations
- **Validation**: COLMAP CLI can verify export correctness before training
- **Community Support**: Extensive documentation and troubleshooting resources
- **Future-Proof**: COLMAP format unlikely to change significantly
- **Tool Integration**: COLMAP GUI can visualise exported cameras

### Negative

- **Complexity**: Binary format requires careful byte-order handling
- **Empty points3D**: Must handle the case where sparse points are not needed
- **Database Optional**: Some tools expect database.db, others don't

### Compatibility Matrix

| Training Tool | COLMAP Support | Notes |
|---------------|----------------|-------|
| graphdeco-inria/gaussian-splatting | Native | Primary target |
| nerfstudio gsplat | Native | Full support |
| instant-ngp | Converter | Via scripts |
| Polycam | N/A | Proprietary |
| 3DGS-MCMC | Native | Recommended |
| Mip-Splatting | Native | Recommended |

## Alternatives Considered

### 1. NeRF Synthetic Format (transforms.json)

**Description**: JSON file with transform matrices per image

```json
{
    "camera_angle_x": 0.6911,
    "frames": [
        {
            "file_path": "./images/frame_00000.png",
            "transform_matrix": [[...], [...], [...], [...]]
        }
    ]
}
```

**Rejected Because**:
- Not native to canonical 3DGS implementation
- Limited camera model support
- No validation tooling
- Declining adoption in 3DGS community

### 2. Custom Binary Format

**Description**: Design a bespoke format optimised for UE5-3DGS workflow

**Rejected Because**:
- Requires writing loaders for every training tool
- No community adoption
- Maintenance burden
- No validation tools

### 3. PLY with Embedded Cameras

**Description**: Store camera data as PLY comments or custom elements

**Rejected Because**:
- Non-standard extension
- PLY parsers would ignore camera data
- No tool support

### 4. Multi-Format Export

**Description**: Export to COLMAP, NeRF Synthetic, and custom formats simultaneously

**Considered For Phase 2**: May add NeRF Synthetic as secondary format if requested

## References

- PRD Section 6.1: COLMAP Format Specification
- COLMAP Documentation: https://colmap.github.io/format.html
- Research: Format Standards (`docs/research/format-standards.md`)
- graphdeco-inria/gaussian-splatting: https://github.com/graphdeco-inria/gaussian-splatting
