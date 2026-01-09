# SPARC Pseudocode Document
## UE5-3DGS: Algorithm Specifications

**Version:** 1.0
**Phase:** Pseudocode
**Status:** Draft

---

## 1. Spherical Trajectory Generation (Fibonacci Lattice)

### 1.1 Algorithm Overview

The Fibonacci lattice provides near-optimal uniform distribution of points on a sphere, superior to random sampling or regular grid approaches for camera placement.

### 1.2 Mathematical Foundation

**Golden Ratio:**
```
phi = (1 + sqrt(5)) / 2 = 1.618033988749895
```

**Fibonacci Lattice Mapping:**
For N points indexed i = 0 to N-1:
```
theta[i] = 2 * PI * i / phi      // Azimuthal angle
phi[i] = acos(1 - 2*(i+0.5)/N)   // Polar angle from top
```

### 1.3 Pseudocode

```cpp
/**
 * Generate uniformly distributed camera positions on sphere using Fibonacci lattice
 *
 * @param NumCameras Number of camera positions to generate
 * @param SphereCenter World position of sphere center
 * @param SphereRadius Distance from center to cameras
 * @param ElevationMin Minimum elevation angle (degrees, -90 to 90)
 * @param ElevationMax Maximum elevation angle (degrees, -90 to 90)
 * @return Array of camera transforms, all looking toward center
 */
ALGORITHM GenerateSphericalTrajectory(
    NumCameras: int,
    SphereCenter: FVector,
    SphereRadius: float,
    ElevationMin: float = -60.0,
    ElevationMax: float = 60.0
) -> TArray<FTransform>

    // Constants
    CONST GoldenRatio = (1.0 + sqrt(5.0)) / 2.0
    CONST PI = 3.14159265358979323846

    // Convert elevation limits to polar angle range
    // Polar angle: 0 = north pole (+Z), PI = south pole (-Z)
    PolarMin = (90.0 - ElevationMax) * PI / 180.0
    PolarMax = (90.0 - ElevationMin) * PI / 180.0

    // Output array
    CameraTransforms: TArray<FTransform>
    CameraTransforms.Reserve(NumCameras)

    FOR i = 0 TO NumCameras - 1 DO
        // Fibonacci lattice indexing
        // Golden angle in radians for azimuth
        AzimuthAngle = 2.0 * PI * i / GoldenRatio

        // Uniform distribution mapping for polar angle
        // Maps [0, 1] -> [PolarMin, PolarMax] with cosine distribution
        T = (i + 0.5) / NumCameras  // [0, 1] range
        CosPolar = 1.0 - 2.0 * T     // [1, -1] range (full sphere)

        // Clamp to elevation range
        CosMin = cos(PolarMin)
        CosMax = cos(PolarMax)
        CosPolar = Clamp(CosPolar, CosMax, CosMin)

        PolarAngle = acos(CosPolar)

        // Spherical to Cartesian conversion
        // UE5: X-forward, Y-right, Z-up
        SinPolar = sin(PolarAngle)

        LocalX = SinPolar * cos(AzimuthAngle)
        LocalY = SinPolar * sin(AzimuthAngle)
        LocalZ = cos(PolarAngle)

        // Camera position on sphere
        Position = SphereCenter + FVector(LocalX, LocalY, LocalZ) * SphereRadius

        // Camera looks toward center
        LookDirection = (SphereCenter - Position).GetSafeNormal()

        // Compute rotation with consistent up vector
        // Handle pole singularity
        IF abs(LocalZ) > 0.999 THEN
            // Near poles, use X as reference up
            UpVector = FVector(1, 0, 0)
        ELSE
            // Use world Z as reference up
            UpVector = FVector(0, 0, 1)
        END IF

        Rotation = MakeRotationFromXZ(LookDirection, UpVector)

        // Create transform
        Transform = FTransform(Rotation, Position, FVector::OneVector)
        CameraTransforms.Add(Transform)
    END FOR

    RETURN CameraTransforms

END ALGORITHM
```

### 1.4 Helper Function: Rotation from Direction

```cpp
/**
 * Create rotation quaternion from forward (X) and up (Z) directions
 */
ALGORITHM MakeRotationFromXZ(
    ForwardDir: FVector,  // Desired X axis
    UpReference: FVector  // Reference up direction
) -> FQuat

    // Normalize forward direction
    X = ForwardDir.GetSafeNormal()

    // Compute right vector (Y axis in UE5)
    Y = (UpReference ^ X).GetSafeNormal()  // Cross product

    // Handle degenerate case (forward parallel to up)
    IF Y.IsNearlyZero() THEN
        // Choose arbitrary perpendicular
        Y = (FVector(0, 1, 0) ^ X).GetSafeNormal()
        IF Y.IsNearlyZero() THEN
            Y = (FVector(0, 0, 1) ^ X).GetSafeNormal()
        END IF
    END IF

    // Recompute up to ensure orthogonality
    Z = (X ^ Y).GetSafeNormal()

    // Build rotation matrix and convert to quaternion
    RotMatrix = FMatrix(
        FPlane(X.X, X.Y, X.Z, 0),
        FPlane(Y.X, Y.Y, Y.Z, 0),
        FPlane(Z.X, Z.Y, Z.Z, 0),
        FPlane(0, 0, 0, 1)
    )

    RETURN RotMatrix.ToQuat()

END ALGORITHM
```

---

## 2. Depth Buffer Conversion (Reverse-Z to Linear)

### 2.1 Background

UE5 uses reverse-Z depth buffer for improved floating-point precision at distance. The depth buffer stores:
```
Z_buffer = FarPlane / (FarPlane - Z_eye)  // Simplified reverse-Z
```

Where Z_eye is the distance from camera in eye/view space (positive forward).

### 2.2 UE5 Projection Matrix

For a perspective projection with reverse-Z infinite far plane:
```
Near = NearClipPlane
Far = FarClipPlane (or infinity)

// UE5 projection (row-major, pre-multiply):
P[2][2] = 0           // Instead of -Far/(Far-Near)
P[2][3] = Near        // Instead of -Far*Near/(Far-Near)
P[3][2] = 1           // Perspective divide marker
```

### 2.3 Pseudocode

```cpp
/**
 * Convert UE5 reverse-Z depth buffer to linear depth in metres
 *
 * @param DepthBuffer Raw depth values from render target [0, 1]
 * @param Width Image width
 * @param Height Image height
 * @param NearPlane Near clip plane in UE5 units (cm)
 * @param FarPlane Far clip plane in UE5 units (cm), or 0 for infinite
 * @param bConvertToMetres Whether to scale output to metres
 * @return Linear depth buffer in metres (or cm if bConvertToMetres=false)
 */
ALGORITHM ConvertReverseZToLinear(
    DepthBuffer: TArray<float>,
    Width: int,
    Height: int,
    NearPlane: float,
    FarPlane: float,
    bConvertToMetres: bool = true
) -> TArray<float>

    LinearDepth: TArray<float>
    LinearDepth.SetNum(Width * Height)

    // Scale factor: cm to metres
    Scale = bConvertToMetres ? 0.01 : 1.0

    // Handle infinite far plane case (common in UE5)
    bInfiniteFar = (FarPlane <= 0.0 OR FarPlane > 1e10)

    FOR y = 0 TO Height - 1 DO
        FOR x = 0 TO Width - 1 DO
            Index = y * Width + x

            // Raw depth value [0, 1]
            // In reverse-Z: 1.0 = near plane, 0.0 = far plane
            RawDepth = DepthBuffer[Index]

            // Handle edge cases
            IF RawDepth <= 0.0 THEN
                // At or beyond far plane
                IF bInfiniteFar THEN
                    LinearDepth[Index] = MAX_FLOAT
                ELSE
                    LinearDepth[Index] = FarPlane * Scale
                END IF
                CONTINUE
            END IF

            IF RawDepth >= 1.0 THEN
                // At near plane
                LinearDepth[Index] = NearPlane * Scale
                CONTINUE
            END IF

            // Reverse-Z to linear conversion
            IF bInfiniteFar THEN
                // Infinite far plane: Z_buffer = Near / Z_eye
                Z_eye = NearPlane / RawDepth
            ELSE
                // Finite far plane: Z_buffer = Far * Near / (Far - Z_eye * (Far - Near))
                // Solving for Z_eye:
                // Z_buffer * (Far - Z_eye * (Far - Near)) = Far * Near
                // Z_buffer * Far - Z_buffer * Z_eye * (Far - Near) = Far * Near
                // Z_eye = (Z_buffer * Far - Far * Near) / (Z_buffer * (Far - Near))
                // Z_eye = Far * (Z_buffer - Near) / (Z_buffer * (Far - Near))

                // Simplified form:
                Z_eye = (FarPlane * NearPlane) /
                        (FarPlane - RawDepth * (FarPlane - NearPlane))
            END IF

            LinearDepth[Index] = Z_eye * Scale
        END FOR
    END FOR

    RETURN LinearDepth

END ALGORITHM
```

### 2.4 GPU Shader Implementation

```hlsl
// DepthExtraction.usf
// GPU-accelerated depth conversion

float3 ConvertDepthToLinear(float RawDepth, float NearPlane, float bInfiniteFar)
{
    // Reverse-Z: 1 = near, 0 = far
    if (RawDepth <= 0.0)
        return 1e10; // Far plane

    if (bInfiniteFar > 0.5)
    {
        // Infinite far plane
        return NearPlane / RawDepth;
    }
    else
    {
        // This path requires FarPlane passed as parameter
        // Handled in calling code
        return NearPlane / RawDepth;
    }
}

// Material function for depth visualization
float3 VisualizeDepth(float LinearDepth, float MinDepth, float MaxDepth)
{
    float NormalizedDepth = saturate((LinearDepth - MinDepth) / (MaxDepth - MinDepth));

    // Turbo colormap approximation for depth visualization
    float4 Coefficients = float4(
        0.1140890109226559,
        0.06288340699912215,
        0.2248337216805064,
        0.5765070529753406
    );

    return float3(
        sin(NormalizedDepth * 6.28318 + 0.0) * 0.5 + 0.5,
        sin(NormalizedDepth * 6.28318 + 2.094) * 0.5 + 0.5,
        sin(NormalizedDepth * 6.28318 + 4.188) * 0.5 + 0.5
    );
}
```

---

## 3. COLMAP Format Writing

### 3.1 Overview

COLMAP uses two format variants:
- **Text format:** Human-readable, useful for debugging
- **Binary format:** Compact, faster to parse

### 3.2 Cameras File Writer

```cpp
/**
 * Write COLMAP cameras.txt file
 *
 * @param FilePath Output file path
 * @param Cameras Array of unique camera configurations
 * @return True if successful
 */
ALGORITHM WriteCamerasText(
    FilePath: FString,
    Cameras: TArray<FCOLMAPCamera>
) -> bool

    // Open file for writing
    FileHandle = OpenFile(FilePath, WRITE_MODE)
    IF NOT FileHandle.IsValid() THEN
        RETURN false
    END IF

    // Write header comment
    FileHandle.WriteLine("# Camera list with one line of data per camera:")
    FileHandle.WriteLine("#   CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]")
    FileHandle.WriteLine("# Number of cameras: " + ToString(Cameras.Num()))

    FOR Camera IN Cameras DO
        // Format line based on camera model
        Line = ""

        SWITCH Camera.Model
            CASE PINHOLE:
                // CAMERA_ID MODEL WIDTH HEIGHT fx fy cx cy
                Line = Format("%d PINHOLE %d %d %.17g %.17g %.17g %.17g",
                    Camera.CameraID,
                    Camera.Width,
                    Camera.Height,
                    Camera.FocalX,
                    Camera.FocalY,
                    Camera.PrincipalX,
                    Camera.PrincipalY)

            CASE SIMPLE_PINHOLE:
                // CAMERA_ID MODEL WIDTH HEIGHT f cx cy
                Line = Format("%d SIMPLE_PINHOLE %d %d %.17g %.17g %.17g",
                    Camera.CameraID,
                    Camera.Width,
                    Camera.Height,
                    Camera.FocalX,  // Assume FocalX == FocalY
                    Camera.PrincipalX,
                    Camera.PrincipalY)

            CASE OPENCV:
                // CAMERA_ID MODEL WIDTH HEIGHT fx fy cx cy k1 k2 p1 p2
                Line = Format("%d OPENCV %d %d %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g",
                    Camera.CameraID,
                    Camera.Width,
                    Camera.Height,
                    Camera.FocalX,
                    Camera.FocalY,
                    Camera.PrincipalX,
                    Camera.PrincipalY,
                    Camera.K1, Camera.K2,
                    Camera.P1, Camera.P2)
        END SWITCH

        FileHandle.WriteLine(Line)
    END FOR

    FileHandle.Close()
    RETURN true

END ALGORITHM
```

### 3.3 Cameras Binary Writer

```cpp
/**
 * Write COLMAP cameras.bin file
 *
 * Binary format (little-endian):
 * - uint64_t num_cameras
 * - For each camera:
 *   - uint32_t camera_id
 *   - int32_t model_id
 *   - uint64_t width
 *   - uint64_t height
 *   - double params[] (length depends on model)
 */
ALGORITHM WriteCamerasBinary(
    FilePath: FString,
    Cameras: TArray<FCOLMAPCamera>
) -> bool

    FileHandle = OpenFile(FilePath, WRITE_BINARY_MODE)
    IF NOT FileHandle.IsValid() THEN
        RETURN false
    END IF

    // Camera model ID mapping
    ModelID = {
        SIMPLE_PINHOLE: 0,
        PINHOLE: 1,
        SIMPLE_RADIAL: 2,
        RADIAL: 3,
        OPENCV: 4,
        OPENCV_FISHEYE: 5,
        // ... others
    }

    // Parameters count per model
    ParamsCount = {
        SIMPLE_PINHOLE: 3,  // f, cx, cy
        PINHOLE: 4,         // fx, fy, cx, cy
        OPENCV: 8,          // fx, fy, cx, cy, k1, k2, p1, p2
        // ... others
    }

    // Write camera count
    NumCameras = uint64(Cameras.Num())
    FileHandle.WriteBytes(&NumCameras, 8)

    FOR Camera IN Cameras DO
        // Camera ID (uint32)
        CameraID = uint32(Camera.CameraID)
        FileHandle.WriteBytes(&CameraID, 4)

        // Model ID (int32)
        Model = int32(ModelID[Camera.Model])
        FileHandle.WriteBytes(&Model, 4)

        // Width and Height (uint64)
        Width = uint64(Camera.Width)
        Height = uint64(Camera.Height)
        FileHandle.WriteBytes(&Width, 8)
        FileHandle.WriteBytes(&Height, 8)

        // Parameters (double array)
        SWITCH Camera.Model
            CASE PINHOLE:
                Params = [Camera.FocalX, Camera.FocalY,
                          Camera.PrincipalX, Camera.PrincipalY]
            CASE OPENCV:
                Params = [Camera.FocalX, Camera.FocalY,
                          Camera.PrincipalX, Camera.PrincipalY,
                          Camera.K1, Camera.K2, Camera.P1, Camera.P2]
        END SWITCH

        FOR Param IN Params DO
            ParamDouble = double(Param)
            FileHandle.WriteBytes(&ParamDouble, 8)
        END FOR
    END FOR

    FileHandle.Close()
    RETURN true

END ALGORITHM
```

### 3.4 Images File Writer

```cpp
/**
 * Write COLMAP images.txt file
 *
 * Format:
 * IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME
 * POINTS2D[] as (X, Y, POINT3D_ID)
 *
 * Note: UE5-3DGS exports with empty POINTS2D since we don't run SfM
 */
ALGORITHM WriteImagesText(
    FilePath: FString,
    Images: TArray<FCOLMAPImage>
) -> bool

    FileHandle = OpenFile(FilePath, WRITE_MODE)
    IF NOT FileHandle.IsValid() THEN
        RETURN false
    END IF

    // Write header
    FileHandle.WriteLine("# Image list with two lines of data per image:")
    FileHandle.WriteLine("#   IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME")
    FileHandle.WriteLine("#   POINTS2D[] as (X, Y, POINT3D_ID)")
    FileHandle.WriteLine("# Number of images: " + ToString(Images.Num()) +
                        ", mean observations per image: 0")

    FOR Image IN Images DO
        // COLMAP quaternion is world-to-camera, scalar-first (w, x, y, z)
        // Extract quaternion components
        QW = Image.Rotation.W
        QX = Image.Rotation.X
        QY = Image.Rotation.Y
        QZ = Image.Rotation.Z

        // Translation (world-to-camera transform)
        TX = Image.Translation.X
        TY = Image.Translation.Y
        TZ = Image.Translation.Z

        // First line: image pose data
        // Use %.17g for full double precision
        Line1 = Format("%d %.17g %.17g %.17g %.17g %.17g %.17g %.17g %d %s",
            Image.ImageID,
            QW, QX, QY, QZ,
            TX, TY, TZ,
            Image.CameraID,
            Image.Name)
        FileHandle.WriteLine(Line1)

        // Second line: 2D point observations (empty for synthetic data)
        FileHandle.WriteLine("")  // Empty line = no POINTS2D
    END FOR

    FileHandle.Close()
    RETURN true

END ALGORITHM
```

### 3.5 Images Binary Writer

```cpp
/**
 * Write COLMAP images.bin file
 *
 * Binary format:
 * - uint64_t num_images
 * - For each image:
 *   - uint32_t image_id
 *   - double qw, qx, qy, qz (quaternion)
 *   - double tx, ty, tz (translation)
 *   - uint32_t camera_id
 *   - char* name (null-terminated string)
 *   - uint64_t num_points2d
 *   - For each point2d:
 *     - double x, y
 *     - uint64_t point3d_id (-1 if not triangulated)
 */
ALGORITHM WriteImagesBinary(
    FilePath: FString,
    Images: TArray<FCOLMAPImage>
) -> bool

    FileHandle = OpenFile(FilePath, WRITE_BINARY_MODE)
    IF NOT FileHandle.IsValid() THEN
        RETURN false
    END IF

    // Write image count
    NumImages = uint64(Images.Num())
    FileHandle.WriteBytes(&NumImages, 8)

    FOR Image IN Images DO
        // Image ID (uint32)
        ImageID = uint32(Image.ImageID)
        FileHandle.WriteBytes(&ImageID, 4)

        // Quaternion (4 x double) - scalar first: w, x, y, z
        QW = double(Image.Rotation.W)
        QX = double(Image.Rotation.X)
        QY = double(Image.Rotation.Y)
        QZ = double(Image.Rotation.Z)
        FileHandle.WriteBytes(&QW, 8)
        FileHandle.WriteBytes(&QX, 8)
        FileHandle.WriteBytes(&QY, 8)
        FileHandle.WriteBytes(&QZ, 8)

        // Translation (3 x double)
        TX = double(Image.Translation.X)
        TY = double(Image.Translation.Y)
        TZ = double(Image.Translation.Z)
        FileHandle.WriteBytes(&TX, 8)
        FileHandle.WriteBytes(&TY, 8)
        FileHandle.WriteBytes(&TZ, 8)

        // Camera ID (uint32)
        CameraID = uint32(Image.CameraID)
        FileHandle.WriteBytes(&CameraID, 4)

        // Image name (null-terminated string)
        NameBytes = StringToUTF8(Image.Name)
        FileHandle.WriteBytes(NameBytes.GetData(), NameBytes.Num())
        NullTerminator = uint8(0)
        FileHandle.WriteBytes(&NullTerminator, 1)

        // Number of 2D points (uint64) - always 0 for synthetic data
        NumPoints2D = uint64(0)
        FileHandle.WriteBytes(&NumPoints2D, 8)

        // No point2d data since NumPoints2D = 0
    END FOR

    FileHandle.Close()
    RETURN true

END ALGORITHM
```

---

## 4. Coordinate System Conversion

### 4.1 Convention Definitions

```
UE5 (Left-handed, Z-up):
  +X: Forward
  +Y: Right
  +Z: Up
  Units: Centimetres
  Quaternion: (X, Y, Z, W)

COLMAP (Right-handed, Camera convention):
  +X: Right
  +Y: Down
  +Z: Forward (into scene)
  Units: Metres (or user-defined)
  Quaternion: (W, X, Y, Z) - world-to-camera

OpenCV (Right-handed):
  +X: Right
  +Y: Down
  +Z: Forward
  Units: Metres
```

### 4.2 UE5 to COLMAP Conversion

```cpp
/**
 * Convert UE5 camera transform to COLMAP extrinsics
 *
 * COLMAP stores world-to-camera transform as:
 * - Quaternion q (w, x, y, z): Rotation from world to camera
 * - Translation t: Position of world origin in camera coordinates
 *
 * Relationship: X_camera = R * X_world + t
 * Where R is rotation matrix from quaternion q
 *
 * @param UE5Transform Camera-to-world transform in UE5 coordinates
 * @param Scale Unit conversion factor (0.01 for cm to m)
 * @return COLMAP extrinsic parameters
 */
ALGORITHM ConvertUE5ToCOLMAP(
    UE5Transform: FTransform,
    Scale: float = 0.01
) -> FCOLMAPExtrinsics

    // Step 1: Extract UE5 camera position and orientation
    UE5Position = UE5Transform.GetLocation()
    UE5Rotation = UE5Transform.GetRotation()  // (X, Y, Z, W)

    // Step 2: Build UE5 rotation matrix (camera-to-world)
    // UE5 camera looks along +X axis
    UE5RotMatrix = UE5Rotation.ToMatrix()

    // Camera axes in world coordinates
    UE5_CamX = UE5RotMatrix.GetColumn(0)  // Camera forward (+X)
    UE5_CamY = UE5RotMatrix.GetColumn(1)  // Camera right (+Y)
    UE5_CamZ = UE5RotMatrix.GetColumn(2)  // Camera up (+Z)

    // Step 3: Convert to COLMAP camera convention
    // COLMAP: +X right, +Y down, +Z forward
    // Need to map UE5 camera axes to COLMAP axes

    // COLMAP camera X = UE5 camera Y (right)
    // COLMAP camera Y = UE5 camera -Z (down)
    // COLMAP camera Z = UE5 camera X (forward)

    COLMAP_CamX = FVector(UE5_CamY.X, UE5_CamY.Y, UE5_CamY.Z)
    COLMAP_CamY = FVector(-UE5_CamZ.X, -UE5_CamZ.Y, -UE5_CamZ.Z)
    COLMAP_CamZ = FVector(UE5_CamX.X, UE5_CamX.Y, UE5_CamX.Z)

    // Step 4: Build camera-to-world matrix in COLMAP world convention
    // Also need to convert world axes: UE5 (X,Y,Z) -> COLMAP world
    // COLMAP world: typically OpenGL-like or custom
    // For simplicity, assume we keep the same world but convert camera

    // Actually, we need to be more careful here.
    // The standard approach is:
    // 1. Convert UE5 world position to COLMAP world position
    // 2. Convert camera orientation to express in COLMAP convention

    // UE5 world to COLMAP world coordinate transform:
    // COLMAP_X = UE5_Y (UE5 right becomes COLMAP right)
    // COLMAP_Y = -UE5_Z (UE5 up becomes COLMAP down after negation)
    // COLMAP_Z = UE5_X (UE5 forward becomes COLMAP forward)

    // World position conversion
    COLMAP_Position = FVector(
        UE5Position.Y * Scale,    // UE5.Y -> COLMAP.X
        -UE5Position.Z * Scale,   // -UE5.Z -> COLMAP.Y
        UE5Position.X * Scale     // UE5.X -> COLMAP.Z
    )

    // Step 5: Build world-to-camera rotation matrix
    // First, get camera-to-world rotation in COLMAP world coordinates

    // The full transformation is complex. Let's use a cleaner approach:
    // Define the basis change matrices

    // Matrix to convert UE5 world point to COLMAP world point
    M_WorldConvert = [
        [0,  1,  0],   // COLMAP.X = UE5.Y
        [0,  0, -1],   // COLMAP.Y = -UE5.Z
        [1,  0,  0]    // COLMAP.Z = UE5.X
    ] * Scale

    // Matrix to convert UE5 camera local to COLMAP camera local
    // UE5 camera: X=forward, Y=right, Z=up
    // COLMAP camera: X=right, Y=down, Z=forward
    M_CameraConvert = [
        [0,  1,  0],   // COLMAP_cam.X = UE5_cam.Y
        [0,  0, -1],   // COLMAP_cam.Y = -UE5_cam.Z
        [1,  0,  0]    // COLMAP_cam.Z = UE5_cam.X
    ]

    // Get UE5 camera-to-world rotation as 3x3 matrix
    R_UE5_C2W = UE5Rotation.ToMatrix().To3x3()

    // Full conversion:
    // R_COLMAP_C2W = M_WorldConvert * R_UE5_C2W * M_CameraConvert.Transpose()
    R_COLMAP_C2W = M_WorldConvert * R_UE5_C2W * Transpose(M_CameraConvert)

    // COLMAP stores world-to-camera, so invert
    R_COLMAP_W2C = Transpose(R_COLMAP_C2W)  // For rotation matrices, transpose = inverse

    // Step 6: Convert rotation matrix to quaternion
    // COLMAP uses (w, x, y, z) ordering
    COLMAP_Quaternion = MatrixToQuaternion(R_COLMAP_W2C)

    // Ensure w is positive for consistency
    IF COLMAP_Quaternion.W < 0 THEN
        COLMAP_Quaternion = -COLMAP_Quaternion
    END IF

    // Step 7: Compute translation
    // t = -R * C where C is camera center in world coords
    // Already have COLMAP_Position as camera center
    COLMAP_Translation = -(R_COLMAP_W2C * COLMAP_Position)

    // Package result
    Result: FCOLMAPExtrinsics
    Result.Quaternion = FQuat(
        COLMAP_Quaternion.W,
        COLMAP_Quaternion.X,
        COLMAP_Quaternion.Y,
        COLMAP_Quaternion.Z
    )
    Result.Translation = COLMAP_Translation
    Result.CameraCenter = COLMAP_Position

    RETURN Result

END ALGORITHM
```

### 4.3 Simplified Implementation (C++)

```cpp
// Practical C++ implementation
struct FCOLMAPExtrinsics
{
    FQuat Quaternion;    // (W, X, Y, Z) world-to-camera
    FVector Translation; // World origin in camera frame
    FVector CameraCenter; // Camera position in world
};

FCOLMAPExtrinsics ConvertUE5ToCOLMAP(const FTransform& UE5Transform, float Scale)
{
    // UE5 position (cm) to COLMAP position (m)
    const FVector UE5Pos = UE5Transform.GetLocation();
    const FVector COLMAPPos(
        UE5Pos.Y * Scale,     // UE5.Y -> COLMAP.X
        -UE5Pos.Z * Scale,    // -UE5.Z -> COLMAP.Y
        UE5Pos.X * Scale      // UE5.X -> COLMAP.Z
    );

    // UE5 rotation to COLMAP rotation
    // Get forward, right, up vectors
    const FVector Forward = UE5Transform.GetRotation().GetForwardVector();
    const FVector Right = UE5Transform.GetRotation().GetRightVector();
    const FVector Up = UE5Transform.GetRotation().GetUpVector();

    // Build camera-to-world in COLMAP convention
    // Row 0: right vector (COLMAP X)
    // Row 1: -up vector (COLMAP Y)
    // Row 2: forward vector (COLMAP Z)
    FMatrix C2W;
    C2W.M[0][0] = Right.Y;    C2W.M[0][1] = -Right.Z;    C2W.M[0][2] = Right.X;
    C2W.M[1][0] = -Up.Y;      C2W.M[1][1] = Up.Z;        C2W.M[1][2] = -Up.X;
    C2W.M[2][0] = Forward.Y;  C2W.M[2][1] = -Forward.Z;  C2W.M[2][2] = Forward.X;

    // World-to-camera is transpose
    const FMatrix W2C = C2W.GetTransposed();

    // Convert to quaternion (ensure positive W)
    FQuat Q = W2C.ToQuat();
    if (Q.W < 0) Q = -Q;

    // Translation: t = -R * C
    const FVector T = -W2C.TransformVector(COLMAPPos);

    FCOLMAPExtrinsics Result;
    Result.Quaternion = Q;
    Result.Translation = T;
    Result.CameraCenter = COLMAPPos;
    return Result;
}
```

---

## 5. Camera Intrinsic Matrix Computation

### 5.1 From FOV to Intrinsic Matrix

```cpp
/**
 * Compute camera intrinsic matrix from UE5 camera parameters
 *
 * @param HorizontalFOV Horizontal field of view in degrees
 * @param ImageWidth Image width in pixels
 * @param ImageHeight Image height in pixels
 * @param PrincipalPointX Principal point X (default: image center)
 * @param PrincipalPointY Principal point Y (default: image center)
 * @return 3x3 intrinsic matrix K
 */
ALGORITHM ComputeIntrinsicMatrix(
    HorizontalFOV: float,
    ImageWidth: int,
    ImageHeight: int,
    PrincipalPointX: float = -1,
    PrincipalPointY: float = -1
) -> FMatrix3x3

    // Default principal point at image center
    IF PrincipalPointX < 0 THEN
        PrincipalPointX = ImageWidth / 2.0
    END IF
    IF PrincipalPointY < 0 THEN
        PrincipalPointY = ImageHeight / 2.0
    END IF

    // Focal length from horizontal FOV
    // FOV = 2 * atan(W / (2 * fx))
    // fx = W / (2 * tan(FOV/2))
    HalfFOVRadians = (HorizontalFOV * PI / 180.0) / 2.0
    FocalLengthPixels = ImageWidth / (2.0 * tan(HalfFOVRadians))

    // For UE5, we typically assume square pixels
    fx = FocalLengthPixels
    fy = FocalLengthPixels
    cx = PrincipalPointX
    cy = PrincipalPointY

    // Build intrinsic matrix
    // K = [fx  0  cx]
    //     [0  fy  cy]
    //     [0   0   1]
    K: FMatrix3x3
    K.M[0][0] = fx;  K.M[0][1] = 0;   K.M[0][2] = cx;
    K.M[1][0] = 0;   K.M[1][1] = fy;  K.M[1][2] = cy;
    K.M[2][0] = 0;   K.M[2][1] = 0;   K.M[2][2] = 1;

    RETURN K

END ALGORITHM
```

### 5.2 Verification: Reprojection Test

```cpp
/**
 * Verify intrinsic matrix by reprojection test
 *
 * @param K Intrinsic matrix
 * @param WorldPoint 3D point in world coordinates
 * @param CameraTransform Camera world-to-camera transform
 * @param ExpectedPixel Expected 2D pixel location
 * @param Tolerance Maximum allowed pixel error
 * @return True if reprojection is within tolerance
 */
ALGORITHM VerifyReprojection(
    K: FMatrix3x3,
    WorldPoint: FVector,
    CameraTransform: FMatrix4x4,  // World-to-camera
    ExpectedPixel: FVector2D,
    Tolerance: float = 0.5
) -> bool

    // Transform world point to camera coordinates
    CameraPoint = CameraTransform.TransformPosition(WorldPoint)

    // Project to normalized image coordinates
    IF CameraPoint.Z <= 0 THEN
        // Point behind camera
        RETURN false
    END IF

    NormalizedX = CameraPoint.X / CameraPoint.Z
    NormalizedY = CameraPoint.Y / CameraPoint.Z

    // Apply intrinsic matrix
    PixelX = K.M[0][0] * NormalizedX + K.M[0][2]
    PixelY = K.M[1][1] * NormalizedY + K.M[1][2]

    // Compute error
    Error = sqrt((PixelX - ExpectedPixel.X)^2 + (PixelY - ExpectedPixel.Y)^2)

    RETURN Error <= Tolerance

END ALGORITHM
```

---

## 6. Coverage Analysis

### 6.1 Ray-Based Coverage Computation

```cpp
/**
 * Compute scene coverage percentage from camera positions
 *
 * @param CameraTransforms Array of camera transforms
 * @param SceneBounds Scene axis-aligned bounding box
 * @param HorizontalFOV Camera horizontal FOV in degrees
 * @param AspectRatio Image width / height
 * @param VoxelResolution Number of voxels per axis for coverage grid
 * @return Coverage percentage [0, 1]
 */
ALGORITHM ComputeCoverage(
    CameraTransforms: TArray<FTransform>,
    SceneBounds: FBox,
    HorizontalFOV: float,
    AspectRatio: float,
    VoxelResolution: int = 32
) -> float

    // Create coverage voxel grid
    VoxelSize = SceneBounds.GetSize() / VoxelResolution
    CoverageGrid: TArray3D<int>
    CoverageGrid.Init(0, VoxelResolution, VoxelResolution, VoxelResolution)

    // Compute vertical FOV from horizontal FOV and aspect ratio
    HalfHFOV = (HorizontalFOV * PI / 180.0) / 2.0
    HalfVFOV = atan(tan(HalfHFOV) / AspectRatio)

    // For each camera, mark visible voxels
    FOR Transform IN CameraTransforms DO
        CameraPos = Transform.GetLocation()
        Forward = Transform.GetRotation().GetForwardVector()
        Right = Transform.GetRotation().GetRightVector()
        Up = Transform.GetRotation().GetUpVector()

        // For each voxel
        FOR x = 0 TO VoxelResolution - 1 DO
            FOR y = 0 TO VoxelResolution - 1 DO
                FOR z = 0 TO VoxelResolution - 1 DO
                    // Voxel center in world space
                    VoxelCenter = SceneBounds.Min + FVector(
                        (x + 0.5) * VoxelSize.X,
                        (y + 0.5) * VoxelSize.Y,
                        (z + 0.5) * VoxelSize.Z
                    )

                    // Check if voxel is within camera frustum
                    ToVoxel = VoxelCenter - CameraPos
                    Distance = ToVoxel.Size()

                    IF Distance < 0.01 THEN
                        CONTINUE  // Too close
                    END IF

                    Direction = ToVoxel / Distance

                    // Project onto camera axes
                    ForwardDot = Direction | Forward
                    RightDot = Direction | Right
                    UpDot = Direction | Up

                    // Must be in front of camera
                    IF ForwardDot <= 0 THEN
                        CONTINUE
                    END IF

                    // Check horizontal angle
                    HorizontalAngle = atan(RightDot / ForwardDot)
                    IF abs(HorizontalAngle) > HalfHFOV THEN
                        CONTINUE
                    END IF

                    // Check vertical angle
                    VerticalAngle = atan(UpDot / ForwardDot)
                    IF abs(VerticalAngle) > HalfVFOV THEN
                        CONTINUE
                    END IF

                    // Voxel is visible, increment coverage count
                    CoverageGrid[x][y][z] += 1
                END FOR
            END FOR
        END FOR
    END FOR

    // Compute coverage statistics
    TotalVoxels = VoxelResolution^3
    CoveredVoxels = 0
    WellCoveredVoxels = 0  // Seen by 3+ cameras

    FOR x = 0 TO VoxelResolution - 1 DO
        FOR y = 0 TO VoxelResolution - 1 DO
            FOR z = 0 TO VoxelResolution - 1 DO
                IF CoverageGrid[x][y][z] > 0 THEN
                    CoveredVoxels += 1
                END IF
                IF CoverageGrid[x][y][z] >= 3 THEN
                    WellCoveredVoxels += 1
                END IF
            END FOR
        END FOR
    END FOR

    // Return basic coverage ratio
    RETURN float(CoveredVoxels) / float(TotalVoxels)

END ALGORITHM
```

---

## 7. Complexity Analysis

| Algorithm | Time Complexity | Space Complexity | Notes |
|-----------|-----------------|------------------|-------|
| Fibonacci Lattice | O(N) | O(N) | N = number of cameras |
| Depth Conversion | O(W*H) | O(W*H) | W, H = image dimensions |
| COLMAP Text Write | O(N) | O(1) | N = number of images |
| COLMAP Binary Write | O(N) | O(1) | More efficient I/O |
| Coordinate Conversion | O(1) | O(1) | Per-camera operation |
| Coverage Analysis | O(C * V^3) | O(V^3) | C = cameras, V = voxel resolution |

---

*Document Version: 1.0*
*Last Updated: January 2025*
