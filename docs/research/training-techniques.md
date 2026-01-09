# 3D Gaussian Splatting Training Techniques

## Overview

This document covers the latest advances in 3DGS training, focusing on speed, quality, and memory efficiency improvements that are critical for integrating 3DGS with Unreal Engine 5 workflows.

## Training Speed Improvements

### Performance Comparison

| Method | Training Time | Speedup | Quality (PSNR) | Notes |
|--------|--------------|---------|----------------|-------|
| Original 3DGS | 35-45 min | 1x | 27.4 dB | Baseline |
| 3DGS-MCMC | 100-200 sec | 10-15x | 27.6 dB | Stochastic optimization |
| DashGaussian | 5 min | 7-9x | 27.3 dB | Progressive training |
| gsplat | 15-20 min | 2-3x | 27.5 dB | Memory efficient |
| Taming 3DGS | 10-15 min | 3-4x | 27.4 dB | Densification control |

### 3DGS-MCMC (Markov Chain Monte Carlo)

**Key Innovation**: Replaces heuristic densification with stochastic gradient Langevin dynamics.

- **Paper**: [3DGS-MCMC: Markov Chain Monte Carlo for 3D Gaussian Splatting](https://arxiv.org/abs/2404.09591)
- **GitHub**: [ubc-vision/3dgs-mcmc](https://github.com/ubc-vision/3dgs-mcmc)

**Benefits**:
- 10-15x faster training (100-200 seconds typical)
- Principled splat birth/death process
- Better convergence guarantees
- Reduced sensitivity to initialization

**Implementation Notes**:
```python
# Key hyperparameters
opacity_reg = 0.01  # Encourages transparent splats to be pruned
scale_reg = 0.01    # Prevents splat explosion
noise_lr = 5e-5     # Langevin dynamics step size
```

### DashGaussian

**Key Innovation**: Progressive training with adaptive point sampling.

- **Paper**: [DashGaussian: Progressive Gaussian Splatting](https://arxiv.org/abs/2312.17331)

**Training Strategy**:
1. Start with sparse point cloud (10% of final)
2. Progressively add points based on loss gradients
3. Densify in regions with high reconstruction error
4. Prune low-contribution splats continuously

### gsplat Library

**Key Innovation**: Optimized CUDA kernels with 4x memory reduction.

- **GitHub**: [nerfstudio-project/gsplat](https://github.com/nerfstudio-project/gsplat)
- **Documentation**: [gsplat.readthedocs.io](https://gsplat.readthedocs.io)

**Features**:
- Fused forward/backward passes
- Memory-efficient alpha compositing
- Support for large-scale scenes
- PyTorch-native API

**Memory Comparison** (Mip-NeRF 360 scenes):

| Scene | Original 3DGS | gsplat | Reduction |
|-------|---------------|--------|-----------|
| Garden | 24 GB | 6 GB | 4x |
| Bicycle | 18 GB | 4.5 GB | 4x |
| Bonsai | 12 GB | 3 GB | 4x |

## Quality Improvements

### Mip-Splatting

**Key Innovation**: Multi-scale 3D smoothing filter for anti-aliasing.

- **Paper**: [Mip-Splatting: Alias-free 3D Gaussian Splatting](https://arxiv.org/abs/2311.16493)
- **GitHub**: [autonomousvision/mip-splatting](https://github.com/autonomousvision/mip-splatting)

**Quality Metrics**:
| Dataset | PSNR Gain | SSIM Gain |
|---------|-----------|-----------|
| Mip-NeRF 360 | +0.5 dB | +0.01 |
| Deep Blending | +0.8 dB | +0.02 |
| Tanks & Temples | +0.6 dB | +0.01 |

### Spec-Gaussian

**Key Innovation**: Anisotropic spherical Gaussians for specular surfaces.

- **Paper**: [Spec-Gaussian: Anisotropic View-Dependent Appearance for 3D Gaussian Splatting](https://arxiv.org/abs/2402.15870)

**Benefits for UE5**:
- Better handling of reflective materials
- Improved glass/metal rendering
- View-dependent effects without environment probes

### 2D Gaussian Splatting (2DGS)

**Key Innovation**: Flat 2D discs instead of 3D ellipsoids for surface representation.

- **Paper**: [2D Gaussian Splatting for Geometrically Accurate Radiance Fields](https://arxiv.org/abs/2403.17888)
- **GitHub**: [hbb1/2d-gaussian-splatting](https://github.com/hbb1/2d-gaussian-splatting)

**Advantages**:
- Better surface reconstruction
- Cleaner mesh extraction
- Reduced floaters in empty space
- More accurate depth maps

### Scaffold-GS

**Key Innovation**: Hierarchical structure with anchor points.

- **Paper**: [Scaffold-GS: Structured 3D Gaussians for View-Adaptive Rendering](https://arxiv.org/abs/2312.00109)
- **GitHub**: [city-super/Scaffold-GS](https://github.com/city-super/Scaffold-GS)

**Benefits**:
- 10x reduction in storage
- Faster rendering through hierarchical culling
- Better LOD support for large scenes

## Compression Techniques

### Compression Comparison

| Method | Compression Ratio | Quality Loss | Speed Impact |
|--------|-------------------|--------------|--------------|
| CompGS | 27x | -0.3 dB | +5% |
| Reduced 3DGS | 65x | -0.5 dB | +10% |
| HAC | 100x | -0.8 dB | +15% |
| SPZ (Google) | ~90% | ~0 dB | +2% |

### CompGS

**Key Innovation**: Learnable compact representation with codebook.

- **Paper**: [CompGS: Compact 3D Gaussian Splatting](https://arxiv.org/abs/2311.13681)

**Techniques**:
1. Vector quantization of SH coefficients
2. Position quantization with octree
3. Covariance compression via PCA
4. Opacity pruning

### Reduced 3DGS

**Key Innovation**: Aggressive parameter reduction.

- **Paper**: [Reduced 3D Gaussian Splatting](https://arxiv.org/abs/2403.05313)

**Storage Breakdown**:
| Component | Original | Reduced | Savings |
|-----------|----------|---------|---------|
| Position (xyz) | 12 bytes | 6 bytes | 50% |
| Covariance | 24 bytes | 8 bytes | 67% |
| Color (SH) | 192 bytes | 4 bytes | 98% |
| Opacity | 4 bytes | 1 byte | 75% |

## Recommended Training Pipeline for UE5

### Step 1: Data Preparation
```bash
# Use COLMAP for camera calibration
colmap feature_extractor --image_path ./images
colmap exhaustive_matcher --database_path ./database.db
colmap mapper --database_path ./database.db --image_path ./images --output_path ./sparse
```

### Step 2: Training with gsplat (Recommended)
```bash
# Install gsplat
pip install gsplat

# Train with optimized settings
python train.py \
    --data_dir ./sparse \
    --output_dir ./output \
    --iterations 30000 \
    --densify_until_iter 15000 \
    --opacity_reset_interval 3000 \
    --percent_dense 0.01
```

### Step 3: Post-Processing
```python
# Compression and export
from gsplat import compress_gaussians, export_ply

# Apply compression
compressed = compress_gaussians(
    gaussians,
    target_splats=500000,  # For UE5 compatibility
    quantize_sh=True,
    quantize_position=True
)

# Export to PLY
export_ply(compressed, "scene.ply")
```

## Key Papers Reference List

### Essential Reading
1. **Original 3DGS**: [3D Gaussian Splatting for Real-Time Radiance Field Rendering](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/) (SIGGRAPH 2023)
2. **3DGS-MCMC**: [arxiv.org/abs/2404.09591](https://arxiv.org/abs/2404.09591) (2024)
3. **Mip-Splatting**: [arxiv.org/abs/2311.16493](https://arxiv.org/abs/2311.16493) (2024)

### Quality Improvements
4. **2DGS**: [arxiv.org/abs/2403.17888](https://arxiv.org/abs/2403.17888) (2024)
5. **Spec-Gaussian**: [arxiv.org/abs/2402.15870](https://arxiv.org/abs/2402.15870) (2024)
6. **Scaffold-GS**: [arxiv.org/abs/2312.00109](https://arxiv.org/abs/2312.00109) (2023)

### Compression
7. **CompGS**: [arxiv.org/abs/2311.13681](https://arxiv.org/abs/2311.13681) (2023)
8. **Reduced 3DGS**: [arxiv.org/abs/2403.05313](https://arxiv.org/abs/2403.05313) (2024)

### Libraries
9. **gsplat**: [github.com/nerfstudio-project/gsplat](https://github.com/nerfstudio-project/gsplat)
10. **Inria 3DGS**: [github.com/graphdeco-inria/gaussian-splatting](https://github.com/graphdeco-inria/gaussian-splatting)

## Training Best Practices

### Hardware Requirements

| Scene Size | GPU Memory | Training Time (gsplat) |
|------------|------------|------------------------|
| Small (room) | 8 GB | 10-15 min |
| Medium (building) | 16 GB | 20-30 min |
| Large (outdoor) | 24 GB | 45-60 min |
| Massive (city block) | 48 GB+ | 2-4 hours |

### Hyperparameter Guidelines

```yaml
# Recommended starting values
position_lr_init: 0.00016
position_lr_final: 0.0000016
position_lr_delay_mult: 0.01
position_lr_max_steps: 30000
feature_lr: 0.0025
opacity_lr: 0.05
scaling_lr: 0.005
rotation_lr: 0.001
densification_interval: 100
opacity_reset_interval: 3000
densify_grad_threshold: 0.0002
```

### Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| Floaters | Over-densification | Lower densify_grad_threshold |
| Blurry output | Under-densification | Increase iterations |
| Memory overflow | Too many splats | Use gsplat, increase pruning |
| Slow convergence | Poor initialization | Use mesh initialization |
