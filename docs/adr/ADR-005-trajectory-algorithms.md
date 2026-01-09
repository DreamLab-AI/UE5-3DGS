# ADR-005: Camera Trajectory Algorithms

## Status
Accepted

## Context

Optimal 3DGS training requires comprehensive scene coverage from multiple viewpoints. The PRD specifies several trajectory types (Section 4.2.1):

| Trajectory Type | Use Case | Coverage Pattern |
|-----------------|----------|------------------|
| Spherical | Object-centric | Uniform sphere sampling |
| Orbital | Scene overview | Concentric rings at elevations |
| Flythrough | Interior spaces | Path-based navigation |
| Random Stratified | Research | Random with constraints |
| Grid Systematic | Benchmarking | Regular grid sampling |
| Custom Spline | Cinematic | User-defined paths |
| Adaptive Coverage | Advanced | AI-driven gap filling |

Research findings (see `docs/research/camera-trajectories.md`):

- **Minimum Views**: 50-100 for simple objects, 300-500 for complex environments
- **Coverage Target**: >95% scene coverage from 2+ viewpoints
- **Baseline Distribution**: Views should follow log-normal camera baseline distribution
- **Elevation Range**: -30deg to +75deg captures most surfaces

## Decision

We will implement **three primary trajectory algorithms** in Phase 1, with adaptive coverage planned for Phase 2:

### Phase 1 Algorithms

#### 1. Spherical (Fibonacci Lattice)

Mathematically optimal uniform distribution on a sphere, ideal for object-centric capture.

```cpp
class FSCM_SphericalTrajectory : public ISCM_TrajectoryGenerator
{
public:
    TArray<FTransform> GenerateTrajectory(const FSCM_TrajectoryParams& Params) override
    {
        TArray<FTransform> Cameras;
        const float GoldenRatio = (1.0f + FMath::Sqrt(5.0f)) / 2.0f;

        for (int32 i = 0; i < Params.NumViews; ++i)
        {
            // Fibonacci lattice for uniform sphere sampling
            float Theta = 2.0f * PI * i / GoldenRatio;
            float Phi = FMath::Acos(1.0f - 2.0f * (i + 0.5f) / Params.NumViews);

            // Apply elevation constraints
            Phi = FMath::Clamp(Phi,
                FMath::DegreesToRadians(90.0f - Params.MaxElevation),
                FMath::DegreesToRadians(90.0f - Params.MinElevation));

            // Compute position
            FVector Position = Params.Center + FVector(
                Params.Radius * FMath::Sin(Phi) * FMath::Cos(Theta),
                Params.Radius * FMath::Sin(Phi) * FMath::Sin(Theta),
                Params.Radius * FMath::Cos(Phi)
            );

            // Look at centre
            FRotator LookAt = (Params.LookAtTarget - Position).Rotation();

            Cameras.Add(FTransform(LookAt, Position));
        }

        return Cameras;
    }
};
```

**Parameters:**
- `NumViews`: 50-500 (default: 100)
- `Radius`: Distance from centre in UE5 units
- `MinElevation`: Minimum angle from horizon (-60 to 0)
- `MaxElevation`: Maximum angle from horizon (0 to 90)
- `Center`: Scene centroid
- `LookAtTarget`: Point cameras focus on

#### 2. Orbital (Multi-Ring)

Concentric rings at different elevations, provides structured coverage for larger scenes.

```cpp
class FSCM_OrbitalTrajectory : public ISCM_TrajectoryGenerator
{
public:
    TArray<FTransform> GenerateTrajectory(const FSCM_TrajectoryParams& Params) override
    {
        TArray<FTransform> Cameras;

        // Ring configuration (elevation, views per ring)
        TArray<TPair<float, int32>> Rings = {
            {75.0f, 12},   // Top-down
            {45.0f, 24},   // High angle
            {15.0f, 36},   // Slightly above eye level
            {0.0f, 36},    // Eye level
            {-20.0f, 24}   // Low angle
        };

        // Scale ring views to match total view count
        int32 TotalRingViews = 0;
        for (const auto& Ring : Rings) TotalRingViews += Ring.Value;

        float Scale = static_cast<float>(Params.NumViews) / TotalRingViews;

        for (const auto& Ring : Rings)
        {
            float Elevation = Ring.Key;
            int32 ViewsInRing = FMath::RoundToInt(Ring.Value * Scale);

            for (int32 i = 0; i < ViewsInRing; ++i)
            {
                float Azimuth = 360.0f * i / ViewsInRing;

                // Compute position on ring
                float ElevRad = FMath::DegreesToRadians(Elevation);
                float AzimRad = FMath::DegreesToRadians(Azimuth);

                FVector Position = Params.Center + FVector(
                    Params.Radius * FMath::Cos(ElevRad) * FMath::Cos(AzimRad),
                    Params.Radius * FMath::Cos(ElevRad) * FMath::Sin(AzimRad),
                    Params.Radius * FMath::Sin(ElevRad)
                );

                FRotator LookAt = (Params.Center - Position).Rotation();
                Cameras.Add(FTransform(LookAt, Position));
            }
        }

        return Cameras;
    }
};
```

**Ring Configuration Guidelines:**

| Elevation | Views/Ring | Purpose |
|-----------|------------|---------|
| 75deg | 12-18 | Top surfaces |
| 45deg | 24-36 | Upper details |
| 0deg | 36-48 | Primary facades |
| -20deg | 24-36 | Undersides |

#### 3. Adaptive Coverage (Phase 2)

Neural coverage prediction with iterative refinement based on reconstruction quality feedback.

```cpp
class FSCM_AdaptiveTrajectory : public ISCM_TrajectoryGenerator
{
public:
    TArray<FTransform> GenerateTrajectory(const FSCM_TrajectoryParams& Params) override
    {
        // Phase 1: Generate initial coverage
        TArray<FTransform> InitialCameras = GenerateInitialCoverage(Params);

        // Phase 2: Analyse coverage gaps
        FCoverageAnalysis Analysis = AnalyseCoverage(InitialCameras, Params.SceneBounds);

        // Phase 3: Fill gaps with additional cameras
        TArray<FTransform> GapFillers = GenerateGapFillers(Analysis, Params);

        // Combine and return
        InitialCameras.Append(GapFillers);
        return InitialCameras;
    }

private:
    FCoverageAnalysis AnalyseCoverage(
        const TArray<FTransform>& Cameras,
        const FBox& SceneBounds)
    {
        // Voxelise scene bounds
        // Ray-march from each camera
        // Identify regions with <2 views coverage
        // Return gap locations and priorities
    }

    TArray<FTransform> GenerateGapFillers(
        const FCoverageAnalysis& Analysis,
        const FSCM_TrajectoryParams& Params)
    {
        // For each gap region:
        //   - Compute optimal camera position
        //   - Ensure visibility of gap
        //   - Add to trajectory
    }
};
```

### Trajectory Type Selection

```cpp
enum class ESCM_TrajectoryType : uint8
{
    Spherical,          // Object-centric, uniform coverage
    Orbital,            // Multi-ring, structured coverage
    Flythrough,         // Spline-based path
    Random_Stratified,  // Random with coverage constraints
    Grid_Systematic,    // Regular grid
    Custom_Spline,      // User-defined spline
    Adaptive_Coverage   // Phase 2: AI-driven
};

TSharedPtr<ISCM_TrajectoryGenerator> CreateTrajectoryGenerator(ESCM_TrajectoryType Type)
{
    switch (Type)
    {
        case ESCM_TrajectoryType::Spherical:
            return MakeShared<FSCM_SphericalTrajectory>();
        case ESCM_TrajectoryType::Orbital:
            return MakeShared<FSCM_OrbitalTrajectory>();
        case ESCM_TrajectoryType::Adaptive_Coverage:
            return MakeShared<FSCM_AdaptiveTrajectory>();
        // ...
    }
}
```

### Recommended View Counts

| Scene Type | Algorithm | Minimum | Recommended | Maximum |
|------------|-----------|---------|-------------|---------|
| Simple object | Spherical | 50 | 72 | 100 |
| Room interior | Orbital | 100 | 150 | 200 |
| Building exterior | Orbital | 150 | 250 | 400 |
| Large outdoor | Adaptive | 200 | 400 | 800+ |
| 360deg environment | Spherical | 300 | 500 | 1000+ |

## Consequences

### Positive

- **Mathematical Optimality**: Fibonacci lattice provides provably uniform coverage
- **Scene Flexibility**: Different algorithms suit different scene types
- **Predictable Results**: Deterministic trajectories enable reproducibility
- **Performance**: Generation is fast (<1s for 1000 cameras)
- **Research Alignment**: View counts based on published 3DGS research

### Negative

- **Manual Selection**: User must choose appropriate algorithm
- **No Occlusion Handling**: Phase 1 algorithms don't account for scene geometry
- **Fixed Patterns**: May not be optimal for unusual scene configurations

### Coverage Guarantees

| Metric | Target | Validation |
|--------|--------|------------|
| Scene coverage | >95% | Voxel ray-marching analysis |
| Multi-view coverage | >80% from 3+ views | Per-voxel view counting |
| Angular uniformity | <15deg std dev | Spherical histogram |

## Alternatives Considered

### 1. COLMAP-Based Trajectory Estimation

**Description**: Let COLMAP estimate poses from UE5 renders, then refine

**Rejected Because**:
- Defeats the purpose of perfect ground-truth poses
- Adds unnecessary computation
- May introduce pose estimation errors

### 2. Random Camera Placement

**Description**: Uniformly random positions within scene bounds

**Rejected Because**:
- Poor coverage distribution
- Many wasted views
- Unpredictable results
- Research shows structured patterns outperform random

### 3. Single Algorithm (Spherical Only)

**Description**: Use Fibonacci lattice for all scenes

**Rejected Because**:
- Spherical doesn't work for large environments
- Interior spaces need different patterns
- Reduces flexibility

### 4. User-Placed Cameras Only

**Description**: Require manual camera placement

**Rejected Because**:
- Time-consuming for large view counts
- Inconsistent coverage
- Doesn't scale

## References

- PRD Section 4.2.1: Scene Capture Module trajectory specifications
- Research: Camera Trajectories (`docs/research/camera-trajectories.md`)
- Fibonacci Lattice: Gonzalez (2010) "Measurement of Areas on a Sphere Using Fibonacci and Latitude-Longitude Lattices"
- 3DGS View Requirements: Kerbl et al. (2023) supplementary materials
