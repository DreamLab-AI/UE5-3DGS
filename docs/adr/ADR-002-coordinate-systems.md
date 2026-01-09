# ADR-002: Coordinate System Conversion

## Status
Accepted

## Context

Unreal Engine 5 and COLMAP use fundamentally different coordinate systems:

| System | Handedness | Up Axis | Forward Axis | Right Axis |
|--------|------------|---------|--------------|------------|
| UE5 | Left-handed | +Z | +X | +Y |
| COLMAP | Right-handed | -Y | +Z | +X |
| OpenCV | Right-handed | -Y | +Z | +X |
| OpenGL | Right-handed | +Y | -Z | +X |

Additionally:
- UE5 uses centimetres as the default unit; COLMAP expects metres
- UE5 quaternions use (X, Y, Z, W) ordering; COLMAP uses (W, X, Y, Z)
- COLMAP stores world-to-camera transforms; UE5 cameras store camera-to-world

Incorrect coordinate conversion is a **high-probability, high-impact risk** (PRD Section 9.1) that would produce invalid exports incompatible with 3DGS training pipelines.

## Decision

We will implement a **centralised coordinate conversion system** in the Data Extraction Module (DEM) with the following design:

### Coordinate Convention Enumeration

```cpp
enum class EDEM_CoordinateConvention : uint8
{
    UE5_LeftHanded_ZUp,      // Native UE5 (centimetres)
    COLMAP_RightHanded,      // COLMAP native (metres)
    OpenCV_RightHanded,      // OpenCV convention
    OpenGL_RightHanded,      // OpenGL convention
    Blender_RightHanded_ZUp  // Blender convention
};
```

### Transformation Matrix

The UE5-to-COLMAP conversion matrix:

```
        UE5 (X, Y, Z) -> COLMAP (X, Y, Z)

        | 0   1   0 |   | Y_ue5  |   | X_colmap |
        | 0   0  -1 | * | Z_ue5  | = | Y_colmap |
        | 1   0   0 |   | X_ue5  |   | Z_colmap |

Plus:
- Scale: divide by 100 (cm -> m)
- Quaternion: reorder and adjust signs
- Transform: invert (camera-to-world -> world-to-camera)
```

### Core Conversion Functions

```cpp
class FDEM_CoordinateConverter
{
public:
    // Position conversion
    static FVector UE5ToCOLMAP_Position(const FVector& UE5Pos)
    {
        return FVector(
            UE5Pos.Y / 100.0f,   // UE5 Y -> COLMAP X (cm -> m)
           -UE5Pos.Z / 100.0f,   // UE5 Z -> COLMAP -Y
            UE5Pos.X / 100.0f    // UE5 X -> COLMAP Z
        );
    }

    // Quaternion conversion (UE5 XYZW -> COLMAP WXYZ)
    static FQuat UE5ToCOLMAP_Rotation(const FQuat& UE5Quat)
    {
        // Convert coordinate system and reorder
        return FQuat(
            UE5Quat.W,           // W stays as W
            UE5Quat.Y,           // UE5 Y -> COLMAP X
           -UE5Quat.Z,           // UE5 Z -> COLMAP -Y
            UE5Quat.X            // UE5 X -> COLMAP Z
        );
    }

    // Full camera transform (camera-to-world -> world-to-camera)
    static void UE5ToCOLMAP_CameraParams(
        const FTransform& UE5Transform,
        FQuat& OutQ,
        FVector& OutT
    );
};
```

### Validation Strategy

1. **Round-Trip Tests**: Convert UE5 -> COLMAP -> UE5 and verify identity
2. **Known-Point Tests**: Use predetermined points with known COLMAP equivalents
3. **Visual Verification**: Render test scene, export, and verify reprojection

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCoordinateConversionTest,
    "UE5_3DGS.DEM.CoordinateConversion",
    EAutomationTestFlags::ProductFilter
)

bool FCoordinateConversionTest::RunTest(const FString& Parameters)
{
    // Test round-trip accuracy
    FVector OriginalPos(100, 200, 300); // UE5 centimetres

    FVector COLMAPPos = FDEM_CoordinateConverter::UE5ToCOLMAP_Position(OriginalPos);
    FVector RoundTrip = FDEM_CoordinateConverter::COLMAPToUE5_Position(COLMAPPos);

    TestTrue("Position round-trip", OriginalPos.Equals(RoundTrip, 0.001f));
    return true;
}
```

## Consequences

### Positive

- **Single Source of Truth**: All coordinate conversions flow through one module
- **Comprehensive Testing**: Unit tests catch conversion bugs early
- **Multi-Format Support**: Same infrastructure supports OpenCV, OpenGL, Blender conventions
- **Clear Documentation**: Conversion logic is explicit and auditable
- **Round-Trip Verification**: Automated tests ensure mathematical correctness

### Negative

- **Performance Overhead**: Every position/rotation requires conversion (mitigated by batching)
- **Complexity**: Multiple coordinate conventions require careful bookkeeping
- **Learning Curve**: Developers must understand both coordinate systems

### Precision Guarantees

| Metric | Target | Measurement |
|--------|--------|-------------|
| Position accuracy | <1mm error | Round-trip test |
| Rotation accuracy | <0.01 deg error | Quaternion comparison |
| Scale accuracy | Exact (100:1) | Unit conversion |

## Alternatives Considered

### 1. Store in UE5 Coordinates, Convert at Export

**Description**: Keep all internal data in UE5 coordinates, convert only when writing files

**Rejected Because**:
- Inconsistent data representation
- Multiple conversion points increase bug risk
- Harder to debug coordinate issues

### 2. Use OpenCV as Intermediate

**Description**: Convert UE5 -> OpenCV -> COLMAP

**Rejected Because**:
- Extra conversion step introduces error
- OpenCV and COLMAP conventions are nearly identical
- Unnecessary complexity

### 3. Pre-Compute Transformation Matrices

**Description**: Store pre-computed 4x4 transformation matrices for each convention pair

**Rejected Because**:
- Less readable than explicit conversion functions
- Harder to audit correctness
- Matrix multiplication slightly slower than direct component operations

## References

- PRD Section 4.2.2: Data Extraction Module coordinate conversion
- PRD Section 6.1: COLMAP coordinate convention specification
- Research: Format Standards (`docs/research/format-standards.md`)
- COLMAP Documentation: https://colmap.github.io/format.html
