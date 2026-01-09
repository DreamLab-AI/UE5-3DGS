# 360-Degree and VR Support for 3D Gaussian Splatting

## Overview

This document covers methods for creating and rendering 360-degree environments and VR experiences using 3D Gaussian Splatting, with specific focus on integration with Unreal Engine 5 workflows and target VR platforms.

## 360-Degree Capture Methods

### Panoramic 3DGS Techniques

| Method | Paper | Key Innovation | Quality |
|--------|-------|----------------|---------|
| Splatter-360 | [arXiv 2024](https://arxiv.org/abs/2404.00763) | Equirectangular projection | PSNR 32.5 dB |
| 360-GS | [CVPR 2024](https://arxiv.org/abs/2402.18963) | Spherical harmonics optimization | PSNR 33.2 dB |
| ODGS | [arXiv 2024](https://arxiv.org/abs/2403.17638) | Omni-directional training | PSNR 31.8 dB |
| Fisheye-GS | [arXiv 2024](https://arxiv.org/abs/2403.10911) | Fisheye lens modeling | PSNR 30.5 dB |

### Splatter-360

**Paper**: [Splatter-360: Generalizable 360° Gaussian Splatting](https://arxiv.org/abs/2404.00763)

**Key Features**:
- Direct training from equirectangular images
- Handles full 360° field of view
- Supports stereo 360 for VR

**Training Pipeline**:
```python
def train_splatter_360(equirect_images, poses):
    """Train 360-degree Gaussian splatting."""

    # Convert equirectangular to cubemap for training
    cubemaps = [equirect_to_cubemap(img) for img in equirect_images]

    # Initialize Gaussians in spherical coordinates
    gaussians = init_spherical_gaussians(
        num_splats=500000,
        radius_range=(0.1, 100.0)  # Near to far
    )

    # Train with spherical loss
    for iteration in range(30000):
        # Render to equirectangular
        rendered = render_equirect(gaussians, pose)

        # Loss with proper spherical weighting
        loss = spherical_l1_loss(rendered, target) + \
               0.2 * spherical_ssim_loss(rendered, target)

        loss.backward()
        optimizer.step()
```

### 360-GS

**Paper**: [360-GS: Layout-guided Panoramic Gaussian Splatting](https://arxiv.org/abs/2402.18963)

**Key Innovation**: Uses room layout priors for indoor 360 scenes.

**Benefits**:
- Better handling of room corners
- Reduced floaters in empty space
- Improved wall/floor boundaries

### Fisheye-GS

**Paper**: [Fisheye-GS: Lightweight and Extensible 3D Gaussian Splatting](https://arxiv.org/abs/2403.10911)

**Key Features**:
- Supports fisheye and ultra-wide lenses
- Camera distortion modeling
- Efficient for automotive/robotics

## VR Platform Support

### Platform Specifications

| Platform | Display | Resolution | FPS Target | Splat Limit | Format |
|----------|---------|------------|------------|-------------|--------|
| Meta Quest 3 | LCD | 2064x2208/eye | 72-120 Hz | 500K-1M | SPZ |
| Quest Pro | LCD | 1800x1920/eye | 72-90 Hz | 500K-800K | SPZ |
| Apple Vision Pro | Micro-OLED | 3660x3200/eye | 90-100 Hz | 2-5M | PLY/SPZ |
| Varjo XR-4 | Micro-OLED | 3840x3744/eye | 90 Hz | 5-10M | PLY |
| PC VR (Index) | LCD | 1440x1600/eye | 120-144 Hz | 5-10M | PLY |
| WebXR | Variable | Variable | 72-90 Hz | 200K-500K | SOG |

### Meta Quest 3 Optimization

**Target Specifications**:
- 1-2M total splats maximum
- 72 FPS minimum (90 FPS preferred)
- <500 MB memory footprint
- SPZ format for streaming

**Quest 3 Rendering Pipeline**:
```cpp
// Quest 3 optimized renderer (OpenXR)
void RenderGaussians(XrSwapchain swapchain) {
    // Get eye poses
    XrView views[2];
    xrLocateViews(session, &viewState, views);

    for (int eye = 0; eye < 2; eye++) {
        // Frustum culling (critical for performance)
        CullGaussians(views[eye].pose, visibleSplats);

        // Sort back-to-front (GPU parallel sort)
        SortGaussians(visibleSplats, views[eye].pose.position);

        // Render with tile-based rasterization
        RenderTiles(visibleSplats, swapchain, eye);
    }
}
```

### Apple Vision Pro

**Format Support**:
- Native PLY loading via RealityKit
- SPZ streaming for large scenes
- Integration with visionOS spatial computing

**visionOS Integration**:
```swift
// Vision Pro Gaussian Splatting (Swift/RealityKit)
import RealityKit
import GaussianSplatting  // Custom framework

struct GaussianSplattingView: View {
    var body: some View {
        RealityView { content in
            // Load Gaussian splat model
            let splats = try await GaussianSplatModel.load(
                from: URL(string: "scene.spz")!,
                options: .init(
                    maxSplats: 2_000_000,
                    lodEnabled: true
                )
            )

            // Add to scene
            content.add(splats.entity)
        }
        .gesture(SpatialTapGesture())
    }
}
```

### WebXR Implementation

**Format**: SOG (Streaming Optimized Gaussians) or compressed PLY

**Browser Support**:
| Browser | WebXR | WebGPU | Performance |
|---------|-------|--------|-------------|
| Chrome 120+ | Full | Full | Excellent |
| Firefox 120+ | Full | Partial | Good |
| Safari 17+ | Partial | Full | Good |
| Edge 120+ | Full | Full | Excellent |

**WebXR Renderer**:
```typescript
// WebXR Gaussian Splatting viewer
class WebXRGaussianViewer {
    private xrSession: XRSession;
    private gaussians: GaussianSplatData;

    async initXR() {
        // Request immersive session
        this.xrSession = await navigator.xr.requestSession(
            'immersive-vr',
            { requiredFeatures: ['local-floor'] }
        );

        // Load optimized gaussians
        this.gaussians = await loadCompressedGaussians('scene.sog');

        // Start render loop
        this.xrSession.requestAnimationFrame(this.render.bind(this));
    }

    render(time: number, frame: XRFrame) {
        const pose = frame.getViewerPose(this.referenceSpace);

        for (const view of pose.views) {
            // Render Gaussians for each eye
            this.renderGaussiansToView(view);
        }

        this.xrSession.requestAnimationFrame(this.render.bind(this));
    }
}
```

## Quality Targets

### Visual Quality Metrics

| Metric | Minimum | Good | Excellent | VR Threshold |
|--------|---------|------|-----------|--------------|
| PSNR | 28 dB | 32 dB | 35+ dB | 30 dB |
| SSIM | 0.90 | 0.95 | 0.97+ | 0.93 |
| LPIPS | 0.15 | 0.10 | 0.05 | 0.12 |
| FPS | 72 | 90 | 120 | 72+ |

### VR-Specific Quality Considerations

**Stereo Consistency**:
- Both eyes must render identical splats
- View-dependent effects must be smooth
- No flickering between frames

**Latency Requirements**:
- Motion-to-photon: <20ms
- Rendering time: <11ms (90 FPS)
- Splat sorting: <2ms

**Depth Accuracy**:
- Critical for stereo comfort
- Splat size must match depth
- No depth discontinuities

## Export Formats for VR

### SPZ Format (Google)

**Compression**: ~90% size reduction
**Quality Loss**: Negligible (<0.1 dB)

**Structure**:
```
SPZ File Format:
├── Header (32 bytes)
│   ├── Magic: "SPZ1"
│   ├── Version: uint32
│   ├── NumSplats: uint64
│   └── Flags: uint32
├── Codebook (variable)
│   ├── Position codebook
│   ├── Rotation codebook
│   └── SH codebook
├── Compressed data
│   └── VQ indices + entropy coded
└── Footer
    └── Checksum
```

**Usage**:
```python
# Export to SPZ
from spz import compress_to_spz

compress_to_spz(
    input_ply="scene.ply",
    output_spz="scene.spz",
    quality=0.95,  # Quality vs size tradeoff
    chunk_size=65536  # For streaming
)
```

### glTF KHR_gaussian_splatting Extension

**Status**: Proposed extension (2024)

**Specification**:
```json
{
    "extensionsUsed": ["KHR_gaussian_splatting"],
    "extensions": {
        "KHR_gaussian_splatting": {
            "splats": 0,
            "compressionType": "quantized",
            "shBands": 3
        }
    },
    "accessors": [
        {
            "bufferView": 0,
            "componentType": 5126,
            "count": 1000000,
            "type": "VEC3",
            "name": "positions"
        }
    ]
}
```

## 360 Capture from UE5

### Panoramic Capture Setup

```cpp
// UE5 Panoramic Capture Actor
UCLASS()
class APanoramicCapture : public AActor
{
    UPROPERTY()
    TArray<USceneCaptureComponent2D*> CubeCaptures;

    void SetupCubemapCapture()
    {
        // 6 cameras for cubemap faces
        FRotator directions[] = {
            FRotator(0, 0, 0),      // +X (Front)
            FRotator(0, 180, 0),    // -X (Back)
            FRotator(0, 90, 0),     // +Y (Right)
            FRotator(0, -90, 0),    // -Y (Left)
            FRotator(-90, 0, 0),    // +Z (Up)
            FRotator(90, 0, 0)      // -Z (Down)
        };

        for (int i = 0; i < 6; i++)
        {
            auto* Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(
                FName(*FString::Printf(TEXT("CubeCapture_%d"), i))
            );
            Capture->FOVAngle = 90.0f;  // Square faces
            Capture->SetRelativeRotation(directions[i]);
            CubeCaptures.Add(Capture);
        }
    }

    UTexture2D* CaptureEquirectangular(int Width, int Height)
    {
        // Capture all 6 faces
        for (auto* Capture : CubeCaptures)
        {
            Capture->CaptureScene();
        }

        // Convert cubemap to equirectangular
        return CubemapToEquirect(CubeCaptures, Width, Height);
    }
};
```

### Stereo 360 Capture

For VR, capture stereo pairs with eye separation:

```cpp
void AStereo360Capture::CaptureStereoPanorama(float IPD)
{
    // IPD = Interpupillary Distance (typically 0.064m)

    // Capture positions for each angular slice
    const int NumSlices = 64;  // Angular resolution

    for (int slice = 0; slice < NumSlices; slice++)
    {
        float angle = (slice / (float)NumSlices) * 360.0f;

        // Calculate offset for this angle
        FVector leftOffset = FRotator(0, angle, 0).RotateVector(
            FVector(0, -IPD/2, 0)
        );
        FVector rightOffset = FRotator(0, angle, 0).RotateVector(
            FVector(0, IPD/2, 0)
        );

        // Capture left and right eye columns
        CaptureSlice(leftOffset, angle, EyeType::Left);
        CaptureSlice(rightOffset, angle, EyeType::Right);
    }

    // Stitch into stereo equirectangular pair
    StitchToPanorama();
}
```

## Performance Optimization for VR

### LOD Strategy

```
Distance-based LOD levels:
├── LOD 0 (0-2m): Full quality, all splats
├── LOD 1 (2-5m): 50% splats, reduced SH
├── LOD 2 (5-15m): 25% splats, SH band 0 only
└── LOD 3 (15m+): 10% splats, flat color
```

### Foveated Rendering

```python
def apply_foveated_lod(splats, gaze_direction, fov_angles):
    """Apply foveated rendering to Gaussians."""

    # Foveal region (full quality)
    foveal_angle = fov_angles['foveal']  # ~10 degrees
    # Peripheral region (reduced quality)
    peripheral_angle = fov_angles['peripheral']  # ~60 degrees

    for splat in splats:
        angle_to_gaze = angle_between(splat.direction, gaze_direction)

        if angle_to_gaze < foveal_angle:
            splat.lod = 0  # Full quality
        elif angle_to_gaze < peripheral_angle:
            splat.lod = 1  # Medium quality
        else:
            splat.lod = 2  # Low quality / cull
```

## References

### 360 3DGS Papers
1. **Splatter-360**: [arxiv.org/abs/2404.00763](https://arxiv.org/abs/2404.00763)
2. **360-GS**: [arxiv.org/abs/2402.18963](https://arxiv.org/abs/2402.18963)
3. **ODGS**: [arxiv.org/abs/2403.17638](https://arxiv.org/abs/2403.17638)
4. **Fisheye-GS**: [arxiv.org/abs/2403.10911](https://arxiv.org/abs/2403.10911)

### VR Implementation
5. **OpenXR Specification**: [khronos.org/openxr](https://www.khronos.org/openxr/)
6. **Meta Quest Development**: [developer.oculus.com](https://developer.oculus.com/)
7. **visionOS Documentation**: [developer.apple.com/visionos](https://developer.apple.com/visionos/)

### Streaming Formats
8. **SPZ Format**: [Google Research](https://research.google/pubs/)
9. **glTF Extensions**: [github.com/KhronosGroup/glTF](https://github.com/KhronosGroup/glTF)

### WebXR
10. **WebXR API**: [immersiveweb.dev](https://immersiveweb.dev/)
11. **WebGPU Spec**: [w3.org/TR/webgpu](https://www.w3.org/TR/webgpu/)
