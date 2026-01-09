# Architecture Decision Records

This directory contains Architecture Decision Records (ADRs) for the UE5-3DGS project. ADRs document significant architectural decisions made during the development of the in-engine 3D Gaussian Splatting pipeline for Unreal Engine 5.

## ADR Index

| ADR | Title | Status | Date |
|-----|-------|--------|------|
| [ADR-001](ADR-001-plugin-architecture.md) | UE5 Plugin Architecture | Accepted | 2025-01 |
| [ADR-002](ADR-002-coordinate-systems.md) | Coordinate System Conversion | Accepted | 2025-01 |
| [ADR-003](ADR-003-export-format-primary.md) | COLMAP as Primary Export Format | Accepted | 2025-01 |
| [ADR-004](ADR-004-cuda-integration.md) | Optional CUDA Integration | Accepted | 2025-01 |
| [ADR-005](ADR-005-trajectory-algorithms.md) | Camera Trajectory Algorithms | Accepted | 2025-01 |
| [ADR-006](ADR-006-depth-buffer-handling.md) | Depth Buffer Handling | Accepted | 2025-01 |
| [ADR-007](ADR-007-compression-formats.md) | Compression and Streaming Formats | Accepted | 2025-01 |
| [ADR-008](ADR-008-anti-aliasing-integration.md) | Anti-Aliasing Integration | Accepted | 2025-01 |

## ADR Categories

### Core Architecture
- ADR-001: Plugin Architecture (modular SCM/DEM/FCM/TRN)
- ADR-004: CUDA Integration Strategy

### Data Handling
- ADR-002: Coordinate System Conversion
- ADR-006: Depth Buffer Handling

### Export and Formats
- ADR-003: Primary Export Format (COLMAP)
- ADR-007: Compression and Streaming Formats

### Rendering and Quality
- ADR-005: Camera Trajectory Algorithms
- ADR-008: Anti-Aliasing Integration

## How to Read ADRs

Each ADR follows a standard template:

1. **Status**: Current state (Proposed, Accepted, Deprecated, Superseded)
2. **Context**: The problem or requirement driving the decision
3. **Decision**: What we decided and why
4. **Consequences**: Positive and negative impacts
5. **Alternatives Considered**: Other options evaluated

## Creating New ADRs

When creating a new ADR:

1. Use the next available number (ADR-XXX)
2. Follow the template in existing ADRs
3. Update this index file
4. Submit for review before marking as Accepted

## References

- PRD: `/home/devuser/workspace/unreal2gaussian/prd.md`
- Research: `/home/devuser/workspace/unreal2gaussian/docs/research/`
- DDD Documentation: `/home/devuser/workspace/unreal2gaussian/docs/ddd/`
