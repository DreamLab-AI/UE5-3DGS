# Compression and Streaming for 3D Gaussian Splatting

## Overview

Efficient compression and streaming are essential for deploying 3DGS content across different platforms. This document covers state-of-the-art compression techniques, streaming architectures, and platform-specific optimization strategies.

## Compression Techniques

### Compression Method Comparison

| Method | Compression Ratio | PSNR Loss | Encoding Time | Decoding Speed |
|--------|-------------------|-----------|---------------|----------------|
| Original PLY | 1x (baseline) | 0 dB | N/A | Fast |
| Quantized PLY | 3-4x | -0.1 dB | Fast | Fast |
| Vector Quantization | 45x | -0.3 dB | Medium | Fast |
| HAC (Hierarchical) | 100x | -0.8 dB | Slow | Medium |
| SPZ (Google) | ~10x (~90%) | ~0 dB | Fast | Fast |
| Neural Compression | 50-100x | -0.5 dB | Very Slow | Slow |

### Vector Quantization (VQ)

**Principle**: Replace continuous parameters with indices into learned codebooks.

```python
class VectorQuantizer:
    def __init__(self, codebook_size=4096, dim=3):
        self.codebook = nn.Embedding(codebook_size, dim)

    def quantize(self, vectors):
        """Quantize vectors to nearest codebook entry."""
        # Compute distances to all codebook entries
        distances = torch.cdist(vectors, self.codebook.weight)

        # Find nearest codebook entry
        indices = distances.argmin(dim=-1)

        # Return quantized vectors
        return self.codebook(indices), indices

    def encode(self, gaussians):
        """Compress Gaussians using VQ."""
        compressed = {}

        # Position: 3D vectors
        compressed['pos_indices'], _ = self.quantize(gaussians.positions)

        # Rotation: quaternions
        compressed['rot_indices'], _ = self.quantize(gaussians.rotations)

        # Scale: 3D vectors
        compressed['scale_indices'], _ = self.quantize(gaussians.scales)

        # Color SH: multiple codebooks per band
        for band in range(self.num_sh_bands):
            compressed[f'sh{band}_indices'], _ = self.quantize(
                gaussians.sh_coefficients[band]
            )

        return compressed
```

**Compression Breakdown**:

| Component | Original Size | VQ Size | Codebook | Ratio |
|-----------|--------------|---------|----------|-------|
| Position (xyz) | 12 bytes | 2 bytes | 4096 entries | 6x |
| Rotation (quat) | 16 bytes | 2 bytes | 4096 entries | 8x |
| Scale (xyz) | 12 bytes | 2 bytes | 4096 entries | 6x |
| SH (48 coeffs) | 192 bytes | 6 bytes | 3 codebooks | 32x |
| Opacity | 4 bytes | 1 byte | 256 entries | 4x |
| **Total** | **236 bytes** | **13 bytes** | - | **18x** |

### HAC (Hierarchical Anchor Compression)

**Paper**: [HAC: Hash-grid Assisted Context for 3DGS Compression](https://arxiv.org/abs/2403.14530)
**GitHub**: [YihangChen-ee/HAC](https://github.com/YihangChen-ee/HAC)

**Key Innovation**: Uses hash-grid spatial context for entropy coding.

**Compression Pipeline**:
1. Build spatial hash grid
2. Predict parameters from spatial neighbors
3. Encode residuals with context-adaptive entropy coding
4. Hierarchical pruning of redundant splats

**Results**:
| Dataset | Original | HAC Compressed | Ratio |
|---------|----------|----------------|-------|
| Mip-NeRF 360 | 1.2 GB | 12 MB | 100x |
| Tanks & Temples | 800 MB | 8 MB | 100x |
| Deep Blending | 600 MB | 6 MB | 100x |

### SPZ Format (Google)

**Compression**: ~90% reduction with negligible quality loss

**Key Features**:
- Streaming-optimized chunk structure
- Progressive decoding support
- Cross-platform compatibility
- Hardware-friendly decoding

**Format Structure**:
```
SPZ v1.0 Format:
┌─────────────────────────────────────┐
│ Header (32 bytes)                   │
│   - Magic: "SPZ1" (4 bytes)         │
│   - Version: 1 (4 bytes)            │
│   - Num splats: N (8 bytes)         │
│   - Chunk size: 65536 (4 bytes)     │
│   - Compression flags (4 bytes)     │
│   - Reserved (8 bytes)              │
├─────────────────────────────────────┤
│ Global Codebooks                    │
│   - Position codebook (16KB)        │
│   - Rotation codebook (16KB)        │
│   - SH codebooks (48KB)             │
├─────────────────────────────────────┤
│ Chunk 0 (splats 0-65535)            │
│   - Bounding box (24 bytes)         │
│   - Compressed indices              │
│   - Entropy-coded residuals         │
├─────────────────────────────────────┤
│ Chunk 1 (splats 65536-131071)       │
│   ...                               │
├─────────────────────────────────────┤
│ Chunk N                             │
└─────────────────────────────────────┘
```

### SH Coefficient Reduction

Spherical Harmonics often dominate storage. Reducing SH bands provides massive savings.

| SH Bands | Coefficients/Color | Bytes/Splat | Quality Impact |
|----------|-------------------|-------------|----------------|
| 0 (DC only) | 1 | 12 | View-independent color |
| 1 | 4 | 48 | Basic view-dependence |
| 2 | 9 | 108 | Good specular |
| 3 (default) | 16 | 192 | Full specular |

**48x reduction** by using SH band 0 only (for diffuse materials).

## Streaming Architecture

### LOD Streaming System

```
Progressive LOD Structure:
┌────────────────────────────────────────┐
│ Base Layer (LOD 3) - 5-10% of splats   │
│   - Always loaded                       │
│   - Full scene coverage                 │
│   - Low detail everywhere               │
├────────────────────────────────────────┤
│ Detail Layer 1 (LOD 2) - 15% of splats │
│   - Distance < 50m                      │
│   - Medium detail                       │
├────────────────────────────────────────┤
│ Detail Layer 2 (LOD 1) - 30% of splats │
│   - Distance < 20m                      │
│   - High detail                         │
├────────────────────────────────────────┤
│ Detail Layer 3 (LOD 0) - 50% of splats │
│   - Distance < 5m                       │
│   - Full quality                        │
└────────────────────────────────────────┘
```

### Chunk-Based Streaming

**Chunk Configuration**:

| Chunk Size | Splats | Memory | Load Time (100Mbps) |
|------------|--------|--------|---------------------|
| 16K | 16,384 | ~0.5 MB | 40ms |
| 64K | 65,536 | ~2 MB | 160ms |
| 256K | 262,144 | ~8 MB | 640ms |

**Recommended**: 64K splats per chunk for balance of granularity and overhead.

### Streaming Protocol

```python
class GaussianStreamManager:
    def __init__(self, base_url: str):
        self.base_url = base_url
        self.loaded_chunks = {}
        self.pending_requests = []
        self.priority_queue = PriorityQueue()

    async def stream_scene(self, camera_position: vec3):
        """Stream chunks based on camera position."""

        # 1. Always ensure base layer is loaded
        if not self.loaded_chunks.get('base'):
            await self.load_chunk('base', priority='critical')

        # 2. Calculate visible chunks
        visible = self.get_visible_chunks(camera_position)

        # 3. Priority ordering
        for chunk_id in visible:
            distance = self.chunk_distance(chunk_id, camera_position)
            lod = self.distance_to_lod(distance)
            priority = self.calculate_priority(distance, lod)

            self.priority_queue.put((priority, chunk_id, lod))

        # 4. Process priority queue with bandwidth budget
        bandwidth_budget = 10_000_000  # 10 MB/s

        while not self.priority_queue.empty() and bandwidth_budget > 0:
            priority, chunk_id, lod = self.priority_queue.get()

            if not self.is_loaded(chunk_id, lod):
                chunk_size = await self.load_chunk(chunk_id, lod)
                bandwidth_budget -= chunk_size

    async def load_chunk(self, chunk_id: str, lod: int = 0) -> int:
        """Load a single chunk from server."""
        url = f"{self.base_url}/chunks/{chunk_id}_lod{lod}.spz"

        response = await fetch(url)
        chunk_data = decompress_spz(response.data)

        # Merge into scene
        self.merge_chunk(chunk_id, chunk_data)

        return len(response.data)
```

## Platform-Specific Targets

### Splat Count Guidelines

| Platform | Max Splats | Recommended | Memory Budget |
|----------|------------|-------------|---------------|
| Quest 3 | 1M | 500K | 512 MB |
| Quest Pro | 800K | 400K | 384 MB |
| Vision Pro | 5M | 2M | 2 GB |
| PC VR (RTX 3080) | 10M | 5M | 4 GB |
| PC Desktop | 20M | 10M | 8 GB |
| WebGL | 500K | 200K | 256 MB |
| Mobile (iOS/Android) | 300K | 150K | 128 MB |

### Frame Time Budgets

| Platform | Target FPS | Frame Budget | Render Budget |
|----------|------------|--------------|---------------|
| Quest 3 (72Hz) | 72 | 13.9ms | 10ms |
| Quest 3 (90Hz) | 90 | 11.1ms | 8ms |
| Vision Pro | 90 | 11.1ms | 8ms |
| PC VR (90Hz) | 90 | 11.1ms | 9ms |
| PC Desktop | 60 | 16.7ms | 14ms |
| Mobile | 30 | 33.3ms | 28ms |

### Memory Layout Optimization

```cpp
// Cache-friendly splat structure for GPU
struct OptimizedSplat {
    // Pack frequently accessed data together

    // Position + scale (accessed together for culling)
    half3 position;     // 6 bytes
    half3 scale;        // 6 bytes

    // Rotation (accessed during rasterization)
    half4 rotation;     // 8 bytes

    // Color (accessed during shading)
    half4 sh_dc;        // 8 bytes (DC + opacity)

    // Higher SH bands (optional, separate buffer)
    // half sh_rest[45]; // 90 bytes - only for LOD 0
};
// Total: 28 bytes (vs 236 bytes original)
// 8.4x memory reduction
```

## File Size Guidelines

### Size Targets by Platform

| Platform | Scene Size | Splat Count | Compressed Size |
|----------|------------|-------------|-----------------|
| Quest Standalone | Small room | 200K | 2-5 MB |
| Quest Standalone | Large room | 500K | 5-15 MB |
| Quest Standalone | Building | 1M | 15-30 MB |
| PC VR | Building | 5M | 50-100 MB |
| PC VR | City block | 10M | 100-200 MB |
| Web | Demo | 100K | 1-2 MB |
| Web | Full scene | 300K | 3-8 MB |

### Bandwidth Requirements

| Quality Level | Bitrate | Scene Load Time |
|---------------|---------|-----------------|
| Base layer | 1-2 Mbps | 2-5 seconds |
| Full scene (streaming) | 5-10 Mbps | 10-30 seconds |
| Instant load | 50+ Mbps | <5 seconds |

## Compression Pipeline

### Recommended Workflow

```bash
# Step 1: Train high-quality Gaussians
python train.py --data ./images --output ./model.ply

# Step 2: Prune low-contribution splats
python prune.py \
    --input ./model.ply \
    --output ./pruned.ply \
    --target_splats 1000000 \
    --opacity_threshold 0.005

# Step 3: Quantize parameters
python quantize.py \
    --input ./pruned.ply \
    --output ./quantized.ply \
    --position_bits 16 \
    --rotation_bits 12 \
    --scale_bits 12 \
    --sh_bands 2

# Step 4: Apply VQ compression
python compress_vq.py \
    --input ./quantized.ply \
    --output ./compressed.spz \
    --codebook_size 4096 \
    --chunk_size 65536

# Step 5: Generate LOD levels
python generate_lods.py \
    --input ./compressed.spz \
    --output_dir ./lods/ \
    --levels 4 \
    --reduction_per_level 0.5
```

### Quality Verification

```python
def verify_compression_quality(original_ply, compressed_spz, test_cameras):
    """Compare rendering quality before/after compression."""

    original = load_gaussians(original_ply)
    compressed = load_spz(compressed_spz)

    metrics = {
        'psnr': [],
        'ssim': [],
        'lpips': []
    }

    for camera in test_cameras:
        orig_render = render(original, camera)
        comp_render = render(compressed, camera)

        metrics['psnr'].append(calculate_psnr(orig_render, comp_render))
        metrics['ssim'].append(calculate_ssim(orig_render, comp_render))
        metrics['lpips'].append(calculate_lpips(orig_render, comp_render))

    return {
        'mean_psnr': np.mean(metrics['psnr']),
        'mean_ssim': np.mean(metrics['ssim']),
        'mean_lpips': np.mean(metrics['lpips']),
        'size_ratio': os.path.getsize(original_ply) / os.path.getsize(compressed_spz)
    }
```

## References

### Compression Papers
1. **CompGS**: [arxiv.org/abs/2311.13681](https://arxiv.org/abs/2311.13681)
2. **Reduced 3DGS**: [arxiv.org/abs/2403.05313](https://arxiv.org/abs/2403.05313)
3. **HAC**: [arxiv.org/abs/2403.14530](https://arxiv.org/abs/2403.14530)
4. **Compact3D**: [arxiv.org/abs/2311.13681](https://arxiv.org/abs/2311.13681)

### Streaming
5. **Octree-GS**: Hierarchical streaming structure
6. **LOD-GS**: Level-of-detail Gaussian splatting

### Tools
7. **SuperSplat**: [github.com/playcanvas/supersplat](https://github.com/playcanvas/supersplat)
8. **gsplat**: [github.com/nerfstudio-project/gsplat](https://github.com/nerfstudio-project/gsplat)

### Platform SDKs
9. **Meta Quest SDK**: [developer.oculus.com](https://developer.oculus.com/)
10. **WebGPU**: [w3.org/TR/webgpu](https://www.w3.org/TR/webgpu/)
