# Camera Trajectories for Polygon-to-3DGS Conversion

## Overview

Converting Unreal Engine 5 polygon meshes to 3D Gaussian Splatting requires careful camera trajectory planning. This document covers optimal capture patterns, view counts, and initialization strategies for high-quality 3DGS reconstruction.

## Trajectory Patterns

### Orbital Ring Pattern (Recommended for Objects)

The orbital ring pattern places cameras on concentric rings around the scene center at different elevation angles.

```
        Top Ring (60-75 deg elevation)
           ○ ○ ○ ○ ○ ○
         ○           ○
       ○   Mid Ring   ○
         ○           ○
           ○ ○ ○ ○ ○ ○
        (30-45 deg elevation)
         ○           ○
       ○   Eye Level  ○
         ○    (0 deg) ○
           ○ ○ ○ ○ ○ ○
        Low Ring (-15 to -30 deg)
```

### Recommended Ring Configuration

| Elevation Angle | Views per Ring | Coverage | Use Case |
|-----------------|----------------|----------|----------|
| 75° (top-down) | 12-18 | Top surfaces | All scenes |
| 45° (high angle) | 24-36 | Upper details | Complex scenes |
| 0° (eye level) | 36-48 | Main facades | Primary views |
| -30° (low angle) | 24-36 | Undersides | Ground-level objects |
| -60° (bottom-up) | 12-18 | Bottom surfaces | Floating objects only |

### Total View Counts by Scene Type

| Scene Type | Minimum Views | Recommended | Maximum | Notes |
|------------|---------------|-------------|---------|-------|
| Simple object | 50 | 72 | 100 | Single focal point |
| Room interior | 100 | 150 | 200 | Cover all walls/corners |
| Building exterior | 150 | 250 | 400 | All facades + rooftop |
| Large outdoor | 200 | 400 | 800+ | Multiple capture stations |
| 360° environment | 300 | 500 | 1000+ | Full spherical coverage |

### Trajectory Generation Code

```python
import numpy as np

def generate_orbital_trajectory(
    center: np.ndarray,
    radius: float,
    elevations: list[float],  # Degrees
    views_per_ring: list[int],
    look_at: np.ndarray = None
) -> list[dict]:
    """Generate orbital camera trajectory around scene center."""

    if look_at is None:
        look_at = center

    cameras = []

    for elev_deg, n_views in zip(elevations, views_per_ring):
        elev_rad = np.radians(elev_deg)

        for i in range(n_views):
            azim_rad = 2 * np.pi * i / n_views

            # Camera position on sphere
            x = center[0] + radius * np.cos(elev_rad) * np.cos(azim_rad)
            y = center[1] + radius * np.cos(elev_rad) * np.sin(azim_rad)
            z = center[2] + radius * np.sin(elev_rad)

            position = np.array([x, y, z])

            # Compute look-at rotation
            forward = look_at - position
            forward = forward / np.linalg.norm(forward)

            cameras.append({
                'position': position,
                'forward': forward,
                'elevation': elev_deg,
                'azimuth': np.degrees(azim_rad)
            })

    return cameras

# Example usage for UE5 scene
cameras = generate_orbital_trajectory(
    center=np.array([0, 0, 100]),  # UE5 Z-up
    radius=500,  # 5 meters in UE5 units
    elevations=[60, 30, 0, -20],
    views_per_ring=[18, 24, 36, 24]
)
# Total: 102 views
```

## Mesh Initialization Methods

Initializing 3DGS from existing mesh geometry significantly improves training speed and quality.

### Initialization Comparison

| Method | PSNR Improvement | Training Speedup | Complexity |
|--------|------------------|------------------|------------|
| Random point cloud | Baseline | 1x | Low |
| Mesh vertices | +0.5 dB | 1.5x | Low |
| Mesh surface sampling | +1.0 dB | 2x | Medium |
| Mesh2Splat | +1.5 dB | 3x | Medium |
| SuGaR (mesh-based) | +1.2 dB | 2.5x | High |

### Mesh2Splat

**Repository**: [mesh2splat](https://github.com/paulpanwang/Mesh2GS)

Direct conversion from mesh to Gaussian splats:

```python
# Mesh2Splat initialization
def mesh_to_gaussians(mesh, samples_per_face=10):
    """Initialize Gaussians from mesh faces."""

    gaussians = []

    for face in mesh.faces:
        # Sample points on face
        points = sample_triangle(face.vertices, samples_per_face)

        for point in points:
            gaussians.append({
                'position': point,
                'normal': face.normal,
                # Covariance aligned to face
                'scale': estimate_local_scale(mesh, point),
                'rotation': normal_to_rotation(face.normal),
                # Color from vertex colors or texture
                'color': sample_texture(mesh, point),
                'opacity': 1.0
            })

    return gaussians
```

### SuGaR (Surface-Aligned Gaussians)

**Paper**: [SuGaR: Surface-Aligned Gaussian Splatting](https://arxiv.org/abs/2311.12775)
**GitHub**: [Anttwo/SuGaR](https://github.com/Anttwo/SuGaR)

SuGaR extracts meshes from trained Gaussians and can initialize new Gaussians from meshes.

**Workflow for UE5**:
1. Export UE5 mesh to OBJ/FBX
2. Initialize Gaussians on mesh surface
3. Apply regularization to keep splats on surface
4. Fine-tune with rendered views

### MeshGS

**Paper**: [MeshGS: Adaptive Mesh-Integrated Gaussian Splatting](https://arxiv.org/abs/2312.02142)

Hybrid mesh-Gaussian representation:
- Uses mesh for structure
- Gaussians for view-dependent appearance
- Maintains mesh connectivity during optimization

### GaMeS (Gaussian Mesh Splatting)

**Paper**: [GaMeS: Mesh-Based Gaussians for Real-time Large-Scale Driving Simulation](https://arxiv.org/abs/2402.04198)

Designed for large-scale automotive scenes:
- Efficient for driving simulators
- Handles dynamic objects
- LOD-aware Gaussian placement

## Depth Supervision

Adding depth supervision from mesh renders improves reconstruction quality by +0.8 dB PSNR.

### Depth-Supervised Training

```python
def depth_loss(rendered_depth, gt_depth, mask=None):
    """Compute depth supervision loss."""

    if mask is not None:
        rendered_depth = rendered_depth[mask]
        gt_depth = gt_depth[mask]

    # Pearson correlation loss (scale-invariant)
    rendered_mean = rendered_depth.mean()
    gt_mean = gt_depth.mean()

    rendered_centered = rendered_depth - rendered_mean
    gt_centered = gt_depth - gt_mean

    correlation = (rendered_centered * gt_centered).sum()
    correlation /= (rendered_centered.std() * gt_centered.std() * len(rendered_depth))

    return 1 - correlation

# Combined loss
total_loss = rgb_loss + 0.1 * depth_loss + 0.01 * normal_loss
```

### UE5 Depth Export

```cpp
// UE5 Blueprint or C++ for depth export
void ACaptureActor::CaptureDepth()
{
    // Create scene capture component
    USceneCaptureComponent2D* DepthCapture = CreateDefaultSubobject<USceneCaptureComponent2D>("DepthCapture");

    // Configure for depth
    DepthCapture->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;

    // High precision depth
    DepthCapture->TextureTarget = CreateRenderTarget2D(
        1920, 1080, ETextureRenderTargetFormat::RTF_R32f
    );

    // Capture
    DepthCapture->CaptureScene();
}
```

## Material Handling

### PBR to Spherical Harmonics Mapping

3DGS uses spherical harmonics (SH) for view-dependent color, which must be derived from UE5 PBR materials.

| PBR Property | SH Mapping | Notes |
|--------------|------------|-------|
| Base Color | DC term (SH band 0) | Primary color |
| Metallic | Higher SH bands | View-dependent reflections |
| Roughness | SH bandwidth | Smooth = more bands |
| Normal Map | Affects SH orientation | Bake into renders |
| Emissive | Additive DC term | May need HDR handling |

### Material Baking Workflow

```python
def bake_pbr_to_sh(
    base_color: np.ndarray,
    metallic: float,
    roughness: float,
    normal: np.ndarray,
    num_sh_bands: int = 3
) -> np.ndarray:
    """Approximate PBR as SH coefficients."""

    # Number of SH coefficients
    num_coeffs = (num_sh_bands + 1) ** 2
    sh = np.zeros((3, num_coeffs))  # RGB

    # DC term (band 0) - diffuse approximation
    sh[:, 0] = base_color * (1 - metallic) * 0.28  # Lambertian normalization

    # Band 1 - directional variation
    if metallic > 0.5:
        # Metallic surfaces need view-dependent terms
        specular_strength = (1 - roughness) * metallic
        sh[:, 1:4] = base_color[:, np.newaxis] * specular_strength * 0.49

    # Higher bands for sharp reflections
    if roughness < 0.3 and num_sh_bands >= 2:
        sh[:, 4:9] = base_color[:, np.newaxis] * (1 - roughness) * 0.1

    return sh.flatten()
```

## Recommended Capture Settings

### Camera Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| FOV | 60-90° | Balance coverage/distortion |
| Resolution | 1920x1080 minimum | Match training resolution |
| Anti-aliasing | TAA or MSAA 4x | Clean edges for training |
| Motion blur | OFF | Sharp images required |
| Depth of field | OFF | All depths in focus |
| Exposure | Fixed | Consistent lighting |

### UE5 Export Settings

```ini
; DefaultEngine.ini settings for capture
[/Script/Engine.RendererSettings]
r.DefaultFeature.AntiAliasing=2  ; TAA
r.TemporalAACurrentFrameWeight=0.2
r.TemporalAASamples=8
r.MotionBlurQuality=0  ; Disabled
r.DepthOfFieldQuality=0  ; Disabled
```

## Trajectory Optimization Tips

### Coverage Analysis

Ensure adequate coverage with overlap analysis:

```python
def analyze_coverage(cameras, scene_bounds, resolution=100):
    """Analyze camera coverage of scene volume."""

    # Create voxel grid
    grid = np.zeros((resolution, resolution, resolution))

    for camera in cameras:
        # Cast rays through camera frustum
        rays = generate_camera_rays(camera, samples_per_dim=50)

        for ray in rays:
            # March ray through volume
            voxels_hit = raymarch_voxels(ray, scene_bounds, resolution)
            grid[voxels_hit] += 1

    # Analyze coverage
    covered = (grid > 2).sum() / grid.size  # At least 2 views
    well_covered = (grid > 5).sum() / grid.size  # 5+ views

    return {
        'basic_coverage': covered,
        'good_coverage': well_covered,
        'coverage_map': grid
    }
```

### Gap Detection

```python
def find_coverage_gaps(coverage_map, threshold=2):
    """Identify regions needing additional cameras."""

    gaps = coverage_map < threshold

    # Find connected components
    from scipy import ndimage
    labeled, n_gaps = ndimage.label(gaps)

    gap_regions = []
    for i in range(1, n_gaps + 1):
        region = np.where(labeled == i)
        center = np.mean(region, axis=1)
        size = len(region[0])

        if size > 100:  # Significant gap
            gap_regions.append({
                'center': center,
                'size': size,
                'priority': 'high' if size > 500 else 'medium'
            })

    return gap_regions
```

## References

### Camera Trajectory Papers
1. **NeRF Studio**: [nerfstudio.github.io](https://docs.nerf.studio/en/latest/quickstart/custom_dataset.html)
2. **Instant NGP**: Camera path recommendations in [NVIDIA Instant NGP](https://github.com/NVlabs/instant-ngp)

### Mesh Initialization
3. **SuGaR**: [arxiv.org/abs/2311.12775](https://arxiv.org/abs/2311.12775)
4. **MeshGS**: [arxiv.org/abs/2312.02142](https://arxiv.org/abs/2312.02142)
5. **GaMeS**: [arxiv.org/abs/2402.04198](https://arxiv.org/abs/2402.04198)

### Depth Supervision
6. **MonoGS**: Depth-supervised Gaussian splatting
7. **DN-Splatter**: [arxiv.org/abs/2403.17822](https://arxiv.org/abs/2403.17822)

### Tools
8. **COLMAP**: [colmap.github.io](https://colmap.github.io/)
9. **UE5 Movie Render Queue**: [Unreal Documentation](https://docs.unrealengine.com/5.0/en-US/movie-render-queue-in-unreal-engine/)
