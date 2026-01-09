# 3D Gaussian Splatting Format Standards

## Overview

This document covers the file formats and emerging standards for 3D Gaussian Splatting data, including the de-facto PLY format, proposed glTF extensions, and streaming formats.

## PLY Format (Current Standard)

### Format Specification

The PLY (Polygon File Format) is the de-facto standard for 3DGS storage.

**File Size**: ~236 bytes per splat (uncompressed)

**Header Example**:
```
ply
format binary_little_endian 1.0
element vertex 1000000
property float x
property float y
property float z
property float nx
property float ny
property float nz
property float f_dc_0
property float f_dc_1
property float f_dc_2
property float f_rest_0
... (45 more f_rest properties for SH)
property float opacity
property float scale_0
property float scale_1
property float scale_2
property float rot_0
property float rot_1
property float rot_2
property float rot_3
end_header
```

### Attribute Details

| Attribute | Type | Bytes | Description |
|-----------|------|-------|-------------|
| x, y, z | float32 | 12 | Position in world space |
| nx, ny, nz | float32 | 12 | Normal (often unused, set to 0) |
| f_dc_0/1/2 | float32 | 12 | DC spherical harmonic (RGB) |
| f_rest_0..44 | float32 | 180 | Higher SH bands (45 coefficients) |
| opacity | float32 | 4 | Opacity (logit space) |
| scale_0/1/2 | float32 | 12 | Scale in log space |
| rot_0/1/2/3 | float32 | 16 | Rotation quaternion (wxyz) |
| **Total** | - | **236** | Per-splat storage |

### Reading PLY in Python

```python
import numpy as np
from plyfile import PlyData

def load_gaussian_ply(path: str) -> dict:
    """Load Gaussian splat PLY file."""

    plydata = PlyData.read(path)
    vertex = plydata['vertex']

    # Extract positions
    positions = np.stack([
        vertex['x'], vertex['y'], vertex['z']
    ], axis=-1).astype(np.float32)

    # Extract SH DC coefficients
    sh_dc = np.stack([
        vertex['f_dc_0'], vertex['f_dc_1'], vertex['f_dc_2']
    ], axis=-1).astype(np.float32)

    # Extract higher SH bands
    sh_rest = np.zeros((len(vertex), 45), dtype=np.float32)
    for i in range(45):
        sh_rest[:, i] = vertex[f'f_rest_{i}']

    # Extract opacity (convert from logit)
    opacity_logit = vertex['opacity'].astype(np.float32)
    opacity = 1.0 / (1.0 + np.exp(-opacity_logit))

    # Extract scale (convert from log)
    scale_log = np.stack([
        vertex['scale_0'], vertex['scale_1'], vertex['scale_2']
    ], axis=-1).astype(np.float32)
    scale = np.exp(scale_log)

    # Extract rotation quaternion
    rotation = np.stack([
        vertex['rot_0'], vertex['rot_1'],
        vertex['rot_2'], vertex['rot_3']
    ], axis=-1).astype(np.float32)
    # Normalize quaternion
    rotation = rotation / np.linalg.norm(rotation, axis=-1, keepdims=True)

    return {
        'positions': positions,
        'sh_dc': sh_dc,
        'sh_rest': sh_rest,
        'opacity': opacity,
        'scale': scale,
        'rotation': rotation,
        'num_splats': len(vertex)
    }
```

### Writing PLY

```python
def save_gaussian_ply(path: str, gaussians: dict):
    """Save Gaussians to PLY format."""

    num_splats = gaussians['num_splats']

    # Prepare vertex data
    dtype = [
        ('x', 'f4'), ('y', 'f4'), ('z', 'f4'),
        ('nx', 'f4'), ('ny', 'f4'), ('nz', 'f4'),
        ('f_dc_0', 'f4'), ('f_dc_1', 'f4'), ('f_dc_2', 'f4'),
    ]
    for i in range(45):
        dtype.append((f'f_rest_{i}', 'f4'))
    dtype.extend([
        ('opacity', 'f4'),
        ('scale_0', 'f4'), ('scale_1', 'f4'), ('scale_2', 'f4'),
        ('rot_0', 'f4'), ('rot_1', 'f4'), ('rot_2', 'f4'), ('rot_3', 'f4')
    ])

    vertex_data = np.zeros(num_splats, dtype=dtype)

    # Fill position
    vertex_data['x'] = gaussians['positions'][:, 0]
    vertex_data['y'] = gaussians['positions'][:, 1]
    vertex_data['z'] = gaussians['positions'][:, 2]

    # Fill normals (zeros)
    vertex_data['nx'] = 0
    vertex_data['ny'] = 0
    vertex_data['nz'] = 0

    # Fill SH DC
    vertex_data['f_dc_0'] = gaussians['sh_dc'][:, 0]
    vertex_data['f_dc_1'] = gaussians['sh_dc'][:, 1]
    vertex_data['f_dc_2'] = gaussians['sh_dc'][:, 2]

    # Fill SH rest
    for i in range(45):
        vertex_data[f'f_rest_{i}'] = gaussians['sh_rest'][:, i]

    # Fill opacity (convert to logit)
    opacity = np.clip(gaussians['opacity'], 1e-6, 1-1e-6)
    vertex_data['opacity'] = np.log(opacity / (1 - opacity))

    # Fill scale (convert to log)
    vertex_data['scale_0'] = np.log(gaussians['scale'][:, 0])
    vertex_data['scale_1'] = np.log(gaussians['scale'][:, 1])
    vertex_data['scale_2'] = np.log(gaussians['scale'][:, 2])

    # Fill rotation
    vertex_data['rot_0'] = gaussians['rotation'][:, 0]
    vertex_data['rot_1'] = gaussians['rotation'][:, 1]
    vertex_data['rot_2'] = gaussians['rotation'][:, 2]
    vertex_data['rot_3'] = gaussians['rotation'][:, 3]

    # Create PlyElement and write
    vertex_element = PlyElement.describe(vertex_data, 'vertex')
    PlyData([vertex_element], text=False).write(path)
```

## Emerging Standards

### glTF KHR_gaussian_splatting Extension

**Status**: Proposed Khronos extension (2024)
**Spec Draft**: [KHR_gaussian_splatting Proposal](https://github.com/KhronosGroup/glTF/issues/2332)

**Rationale**:
- glTF is the industry standard for 3D assets
- Native support in game engines and web viewers
- Built-in compression and streaming support
- Interoperability with existing toolchains

**Proposed Structure**:

```json
{
    "asset": {
        "version": "2.0",
        "generator": "Gaussian Splat Exporter"
    },
    "extensionsUsed": ["KHR_gaussian_splatting"],
    "extensionsRequired": ["KHR_gaussian_splatting"],
    "extensions": {
        "KHR_gaussian_splatting": {
            "gaussianPrimitives": [
                {
                    "positions": 0,
                    "rotations": 1,
                    "scales": 2,
                    "sphericalHarmonics": {
                        "dc": 3,
                        "rest": 4
                    },
                    "opacities": 5,
                    "compressionMode": "quantized",
                    "shBands": 3
                }
            ]
        }
    },
    "buffers": [
        {
            "uri": "gaussians.bin",
            "byteLength": 47200000
        }
    ],
    "bufferViews": [
        {
            "buffer": 0,
            "byteOffset": 0,
            "byteLength": 12000000,
            "target": 34962
        }
    ],
    "accessors": [
        {
            "bufferView": 0,
            "componentType": 5126,
            "count": 1000000,
            "type": "VEC3",
            "name": "positions"
        },
        {
            "bufferView": 1,
            "componentType": 5126,
            "count": 1000000,
            "type": "VEC4",
            "name": "rotations"
        }
    ],
    "scenes": [
        {
            "nodes": [0]
        }
    ],
    "nodes": [
        {
            "extensions": {
                "KHR_gaussian_splatting": {
                    "gaussianPrimitive": 0
                }
            }
        }
    ]
}
```

### SPZ Format (Google)

**Developer**: Google Research
**Purpose**: Streaming-optimized format with compression

**Key Features**:
- ~90% compression ratio
- Progressive loading support
- Chunk-based streaming
- Hardware-friendly decoding

**Format Specification**:

```
SPZ File Structure:
├── Magic Number: "SPZ1" (4 bytes)
├── Header
│   ├── Version (4 bytes)
│   ├── Splat Count (8 bytes)
│   ├── Chunk Size (4 bytes)
│   ├── Compression Type (4 bytes)
│   ├── SH Bands (4 bytes)
│   └── Reserved (8 bytes)
├── Global Codebooks
│   ├── Position Codebook
│   │   ├── Entry Count (4 bytes)
│   │   └── Entries (n × 12 bytes)
│   ├── Rotation Codebook
│   └── SH Codebooks
├── Chunks (N chunks)
│   ├── Chunk Header
│   │   ├── Splat Count (4 bytes)
│   │   ├── Bounding Box (24 bytes)
│   │   └── Compressed Size (4 bytes)
│   └── Compressed Data
│       ├── Position Indices (entropy coded)
│       ├── Rotation Indices
│       ├── Scale Data
│       ├── SH Indices
│       └── Opacity Data
└── Footer
    └── CRC32 Checksum
```

### USD (Universal Scene Description)

**Developer**: Pixar
**Purpose**: Industry-standard scene format

**Proposed 3DGS Schema**:

```usda
#usda 1.0
(
    defaultPrim = "GaussianSplats"
)

def GaussianSplats "GaussianSplats"
{
    # Core attributes
    point3f[] positions
    quatf[] rotations
    float3[] scales
    float[] opacities

    # Spherical harmonics
    color3f[] shDC
    float[] shRest

    # Metadata
    int shBands = 3
    int splatCount = 1000000

    # LOD support
    rel lodLevels = [
        </GaussianSplats/LOD0>,
        </GaussianSplats/LOD1>,
        </GaussianSplats/LOD2>
    ]
}
```

## Tool Ecosystem

### Training Tools

| Tool | Repository | Description |
|------|------------|-------------|
| graphdeco-inria | [github.com/graphdeco-inria/gaussian-splatting](https://github.com/graphdeco-inria/gaussian-splatting) | Original 3DGS implementation |
| gsplat | [github.com/nerfstudio-project/gsplat](https://github.com/nerfstudio-project/gsplat) | Optimized CUDA kernels |
| 3DGS-MCMC | [github.com/ubc-vision/3dgs-mcmc](https://github.com/ubc-vision/3dgs-mcmc) | Fast training |
| Mip-Splatting | [github.com/autonomousvision/mip-splatting](https://github.com/autonomousvision/mip-splatting) | Anti-aliased 3DGS |

### Viewers

| Tool | Platform | URL |
|------|----------|-----|
| SuperSplat | Web | [playcanvas.com/supersplat](https://playcanvas.com/supersplat) |
| Antimatter15 | Web | [antimatter15.com/splat](https://antimatter15.com/splat/) |
| gsplat.js | Web | [github.com/dylanebert/gsplat.js](https://github.com/dylanebert/gsplat.js) |
| Luma AI | iOS/Web | [lumalabs.ai](https://lumalabs.ai) |

### Conversion Tools

| Tool | Purpose | Repository |
|------|---------|------------|
| SuperSplat Editor | PLY editing/export | [github.com/playcanvas/supersplat](https://github.com/playcanvas/supersplat) |
| ply-to-spz | PLY to SPZ conversion | Google internal |
| gs-to-gltf | PLY to glTF (proposed) | Community development |

## Platform Format Matrix

### Recommended Formats by Platform

| Platform | Primary Format | Secondary | Streaming |
|----------|---------------|-----------|-----------|
| UE5 | PLY | Custom binary | Chunked PLY |
| Unity | PLY | Asset bundle | Addressables |
| Quest | SPZ | Compressed PLY | SPZ chunks |
| Vision Pro | PLY | SPZ | SPZ |
| Web (WebGL) | Compressed PLY | SPZ | Progressive |
| Web (WebGPU) | SPZ | glTF ext | Native |
| PC Desktop | PLY | Any | Optional |

### Format Feature Comparison

| Feature | PLY | SPZ | glTF Ext | USD |
|---------|-----|-----|----------|-----|
| Compression | No | Yes (90%) | Yes | No |
| Streaming | Manual | Native | Partial | No |
| LOD Support | No | Yes | Planned | Yes |
| Animation | No | Planned | Yes | Yes |
| Skinning | No | No | Planned | Yes |
| Industry Standard | De-facto | Emerging | Proposed | Yes |
| Tool Support | Excellent | Limited | Planned | Good |

## File Size Comparison

### 1 Million Splats

| Format | Size | Notes |
|--------|------|-------|
| PLY (uncompressed) | 236 MB | Baseline |
| PLY (quantized) | 60-80 MB | 16-bit positions |
| SPZ | 20-30 MB | Full compression |
| glTF + Draco | 40-60 MB | Estimated |
| Binary custom | 30-50 MB | Engine-specific |

### Recommended Export Settings

```yaml
# UE5 Export Configuration
ue5_export:
  format: ply
  precision: float32
  sh_bands: 3
  include_normals: false
  coordinate_system: z_up_right_handed

# Quest Export Configuration
quest_export:
  format: spz
  compression: high
  sh_bands: 1  # DC only for performance
  max_splats: 500000
  chunk_size: 65536

# Web Export Configuration
web_export:
  format: compressed_ply
  precision: float16
  sh_bands: 2
  max_splats: 200000
  progressive: true
```

## References

### Format Specifications
1. **PLY Format**: [paulbourke.net/dataformats/ply](http://paulbourke.net/dataformats/ply/)
2. **glTF 2.0**: [khronos.org/gltf](https://www.khronos.org/gltf/)
3. **USD**: [openusd.org](https://openusd.org/)

### Extensions
4. **KHR_gaussian_splatting**: [GitHub Issue](https://github.com/KhronosGroup/glTF/issues/2332)

### Tools
5. **SuperSplat**: [github.com/playcanvas/supersplat](https://github.com/playcanvas/supersplat)
6. **gsplat**: [github.com/nerfstudio-project/gsplat](https://github.com/nerfstudio-project/gsplat)
7. **Inria 3DGS**: [github.com/graphdeco-inria/gaussian-splatting](https://github.com/graphdeco-inria/gaussian-splatting)

### Viewers
8. **Antimatter15 Viewer**: [antimatter15.com/splat](https://antimatter15.com/splat/)
9. **gsplat.js**: [github.com/dylanebert/gsplat.js](https://github.com/dylanebert/gsplat.js)
