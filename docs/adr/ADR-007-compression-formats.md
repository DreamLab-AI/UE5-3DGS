# ADR-007: Compression and Streaming Formats

## Status
Accepted

## Context

Trained 3DGS models can be large:

| Scene Complexity | Splat Count | Uncompressed PLY | Target Platforms |
|------------------|-------------|------------------|------------------|
| Simple object | 100K | ~24 MB | All |
| Room | 500K | ~118 MB | PC, Quest |
| Building | 1M | ~236 MB | PC |
| City block | 5M | ~1.2 GB | High-end PC |

Platform constraints:

| Platform | Max Splats | Memory Budget | Bandwidth |
|----------|------------|---------------|-----------|
| Quest 3 | 500K | 512 MB | N/A (standalone) |
| Vision Pro | 2M | 2 GB | N/A |
| WebGL | 200K | 256 MB | 10 Mbps typical |
| PC VR | 5M | 4 GB | Local storage |

Research findings (see `docs/research/compression-streaming.md`):

- **SPZ Format**: Google's streaming format achieves ~90% compression with negligible quality loss
- **glTF Extension**: KHR_gaussian_splatting proposed for industry standardisation
- **LOD Streaming**: Progressive loading enables fast initial display

## Decision

We will implement **SPZ and glTF export** as secondary formats alongside COLMAP:

### Format Support Matrix

| Format | Phase | Use Case | Compression | Streaming |
|--------|-------|----------|-------------|-----------|
| COLMAP | 1 | Training | N/A | No |
| PLY | 1 | Viewer import | No | No |
| SPZ | 2 | Production deployment | ~90% | Yes |
| glTF + KHR_gaussian_splatting | 2 | Web/Engine | ~60% | Planned |

### SPZ Format Implementation

```cpp
class FFCM_SPZWriter
{
public:
    struct FSPZConfig
    {
        int32 ChunkSize = 65536;        // Splats per chunk
        int32 CodebookSize = 4096;       // VQ codebook entries
        bool bEnableProgressiveDecoding = true;
        int32 SHBands = 3;              // 0=DC only, 3=full
        ESPZCompression CompressionLevel = ESPZCompression::High;
    };

    bool WriteSPZ(
        const FString& OutputPath,
        const TArray<F3DGS_Splat>& Splats,
        const FSPZConfig& Config)
    {
        // 1. Build codebooks
        FCodebook PositionCodebook = BuildCodebook(
            ExtractPositions(Splats), Config.CodebookSize);
        FCodebook RotationCodebook = BuildCodebook(
            ExtractRotations(Splats), Config.CodebookSize);
        FCodebook SHCodebook = BuildCodebook(
            ExtractSHCoeffs(Splats), Config.CodebookSize);

        // 2. Write header
        WriteSPZHeader(OutputPath, Splats.Num(), Config);

        // 3. Write codebooks
        WriteCodebook(OutputPath, PositionCodebook, "position");
        WriteCodebook(OutputPath, RotationCodebook, "rotation");
        WriteCodebook(OutputPath, SHCodebook, "sh");

        // 4. Write chunks
        int32 NumChunks = FMath::CeilToInt(
            static_cast<float>(Splats.Num()) / Config.ChunkSize);

        for (int32 ChunkIdx = 0; ChunkIdx < NumChunks; ++ChunkIdx)
        {
            int32 Start = ChunkIdx * Config.ChunkSize;
            int32 End = FMath::Min(Start + Config.ChunkSize, Splats.Num());

            TArrayView<const F3DGS_Splat> ChunkSplats(
                Splats.GetData() + Start, End - Start);

            WriteChunk(OutputPath, ChunkIdx, ChunkSplats,
                PositionCodebook, RotationCodebook, SHCodebook);
        }

        return true;
    }

private:
    void WriteSPZHeader(const FString& Path, int32 NumSplats, const FSPZConfig& Config)
    {
        /*
        SPZ Header (32 bytes):
        - Magic: "SPZ1" (4 bytes)
        - Version: 1 (4 bytes)
        - Num splats: N (8 bytes)
        - Chunk size: 65536 (4 bytes)
        - Compression flags (4 bytes)
        - Reserved (8 bytes)
        */
    }

    void WriteChunk(
        const FString& Path,
        int32 ChunkIdx,
        TArrayView<const F3DGS_Splat> Splats,
        const FCodebook& PosCodebook,
        const FCodebook& RotCodebook,
        const FCodebook& SHCodebook)
    {
        // Compute bounding box
        FBox ChunkBounds = ComputeBounds(Splats);

        // Quantize to codebook indices
        TArray<uint16> PosIndices = QuantizeToCodebook(
            ExtractPositions(Splats), PosCodebook);
        TArray<uint16> RotIndices = QuantizeToCodebook(
            ExtractRotations(Splats), RotCodebook);

        // Entropy encode indices
        TArray<uint8> CompressedData = EntropyEncode(
            PosIndices, RotIndices, /* ... */);

        // Write chunk
        WriteChunkData(Path, ChunkIdx, ChunkBounds, CompressedData);
    }
};
```

### glTF KHR_gaussian_splatting Export

```cpp
class FFCM_GLTFWriter
{
public:
    bool WriteGLTF(
        const FString& OutputPath,
        const TArray<F3DGS_Splat>& Splats,
        const FGLTFConfig& Config)
    {
        TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

        // Asset info
        TSharedPtr<FJsonObject> Asset = MakeShared<FJsonObject>();
        Asset->SetStringField("version", "2.0");
        Asset->SetStringField("generator", "UE5-3DGS");
        Root->SetObjectField("asset", Asset);

        // Extensions
        TArray<TSharedPtr<FJsonValue>> ExtensionsUsed;
        ExtensionsUsed.Add(MakeShared<FJsonValueString>("KHR_gaussian_splatting"));
        Root->SetArrayField("extensionsUsed", ExtensionsUsed);
        Root->SetArrayField("extensionsRequired", ExtensionsUsed);

        // Gaussian primitives extension
        TSharedPtr<FJsonObject> Extensions = MakeShared<FJsonObject>();
        TSharedPtr<FJsonObject> GaussianExt = MakeShared<FJsonObject>();

        TArray<TSharedPtr<FJsonValue>> GaussianPrimitives;
        TSharedPtr<FJsonObject> Primitive = MakeShared<FJsonObject>();
        Primitive->SetNumberField("positions", 0);      // Accessor index
        Primitive->SetNumberField("rotations", 1);
        Primitive->SetNumberField("scales", 2);
        Primitive->SetNumberField("opacities", 3);

        TSharedPtr<FJsonObject> SH = MakeShared<FJsonObject>();
        SH->SetNumberField("dc", 4);
        SH->SetNumberField("rest", 5);
        Primitive->SetObjectField("sphericalHarmonics", SH);

        Primitive->SetStringField("compressionMode", "quantized");
        Primitive->SetNumberField("shBands", Config.SHBands);

        GaussianPrimitives.Add(MakeShared<FJsonValueObject>(Primitive));
        GaussianExt->SetArrayField("gaussianPrimitives", GaussianPrimitives);
        Extensions->SetObjectField("KHR_gaussian_splatting", GaussianExt);
        Root->SetObjectField("extensions", Extensions);

        // Write binary buffer
        TArray<uint8> BinaryBuffer;
        WriteGaussianBuffer(BinaryBuffer, Splats, Config);

        // Write accessors, bufferViews, buffers
        WriteAccessors(Root, Splats.Num());
        WriteBufferViews(Root, BinaryBuffer.Num());
        WriteBuffers(Root, OutputPath, BinaryBuffer);

        // Serialize JSON
        FString JsonString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
        FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

        FFileHelper::SaveStringToFile(JsonString, *(OutputPath / TEXT("scene.gltf")));

        return true;
    }
};
```

### Compression Ratios

| Method | Ratio | PSNR Loss | Speed |
|--------|-------|-----------|-------|
| Uncompressed PLY | 1x | 0 dB | Baseline |
| Quantised PLY (16-bit) | 3-4x | -0.1 dB | Fast |
| SPZ | ~10x | ~0 dB | Fast |
| glTF + Draco | ~6x | -0.2 dB | Medium |

### Export Configuration

```cpp
struct FFCM_ExportConfig
{
    // Primary format (always exported)
    bool bExportCOLMAP = true;
    bool bExportPLY = true;

    // Secondary formats (optional)
    bool bExportSPZ = false;
    bool bExportGLTF = false;

    // SPZ settings
    int32 SPZ_ChunkSize = 65536;
    int32 SPZ_SHBands = 3;

    // glTF settings
    bool GLTF_UseDraco = true;
    int32 GLTF_QuantisationBits = 14;
};
```

### Manifest Integration

```json
{
    "version": "1.0",
    "generator": "UE5-3DGS",
    "exports": {
        "colmap": {
            "path": "sparse/0/",
            "format": "binary"
        },
        "ply": {
            "path": "point_cloud.ply",
            "splat_count": 1000000
        },
        "spz": {
            "path": "compressed.spz",
            "chunk_size": 65536,
            "compression_ratio": 9.8,
            "streaming_enabled": true
        },
        "gltf": {
            "path": "scene.gltf",
            "binary_path": "scene.bin",
            "extension": "KHR_gaussian_splatting"
        }
    },
    "recommended_platforms": {
        "quest": "spz",
        "web": "gltf",
        "desktop": "ply"
    }
}
```

## Consequences

### Positive

- **Platform Coverage**: SPZ for mobile VR, glTF for web, PLY for desktop
- **Streaming Ready**: SPZ chunks enable progressive loading
- **Industry Alignment**: glTF follows Khronos standardisation path
- **Quality Preservation**: ~90% size reduction with minimal quality loss
- **Future-Proof**: Both formats have active development communities

### Negative

- **Implementation Effort**: Two additional export pipelines
- **Standard Evolution**: glTF extension not yet finalised
- **Testing Complexity**: Must validate across multiple formats
- **Documentation**: Users need guidance on format selection

### Size Comparison (1M Splats)

| Format | Size | Load Time (100Mbps) |
|--------|------|---------------------|
| PLY | 236 MB | 19s |
| SPZ | ~24 MB | 2s |
| glTF | ~40 MB | 3s |

## Alternatives Considered

### 1. Custom Binary Format

**Description**: Design UE5-3DGS-specific compressed format

**Rejected Because**:
- No ecosystem support
- Requires custom loaders everywhere
- Maintenance burden
- No community adoption path

### 2. ZIP-Compressed PLY

**Description**: Standard PLY with gzip/deflate compression

**Rejected Because**:
- Limited compression ratio (~2-3x)
- No streaming support
- Not optimised for Gaussian data

### 3. USD Integration

**Description**: Pixar Universal Scene Description

**Deferred to Future**:
- No current 3DGS USD schema
- Complex integration
- May revisit when USD schema emerges

### 4. WebP for Images + Separate Geometry

**Description**: Split image and geometry compression

**Rejected Because**:
- 3DGS doesn't use images post-training
- Would require reassembly
- Not standard approach

## References

- PRD Section 6: Export Format Specifications
- Research: Compression and Streaming (`docs/research/compression-streaming.md`)
- Research: Format Standards (`docs/research/format-standards.md`)
- SPZ Format: Google Research internal specification
- glTF KHR_gaussian_splatting: https://github.com/KhronosGroup/glTF/issues/2332
