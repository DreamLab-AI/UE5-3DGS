# UE5-3DGS Documentation

Comprehensive documentation for the Unreal Engine 5 to 3D Gaussian Splatting pipeline.

## Overview

**UE5-3DGS** is an in-engine solution for exporting Unreal Engine 5 scenes to 3D Gaussian Splatting format. The pipeline automates camera trajectory generation, high-fidelity frame capture, COLMAP processing, and 3DGS training to produce viewable splat files (.ply, .spz) directly from UE5 content.

---

## Documentation Structure

```
docs/
├── specification/    # Requirements and constraints
├── ddd/             # Domain-Driven Design models
├── adr/             # Architecture Decision Records
├── sparc/           # SPARC methodology phases
├── agents/          # Agentic development configs
└── research/        # 3DGS research findings
```

---

## Quick Links

### User Documentation

| Document | Description |
|----------|-------------|
| [User Guide](guides/user-guide.md) | Step-by-step usage instructions |
| [Troubleshooting](guides/troubleshooting.md) | Common issues and solutions |

### Technical Reference

| Document | Description |
|----------|-------------|
| [Architecture Guide](architecture/README.md) | System design and module interactions |
| [API Reference](api/README.md) | Complete API documentation |
| [Format Specifications](reference/formats.md) | COLMAP and PLY format details |
| [Coordinate Systems](reference/coordinates.md) | Transformation mathematics |

### Specification
| Document | Description |
|----------|-------------|
| [requirements.md](specification/requirements.md) | Functional requirements |
| [non-functional.md](specification/non-functional.md) | Performance, scalability, reliability |
| [constraints.md](specification/constraints.md) | Technical and platform constraints |
| [edge-cases.md](specification/edge-cases.md) | Error handling scenarios |
| [glossary.md](specification/glossary.md) | Domain terminology |

### Domain-Driven Design
| Document | Description |
|----------|-------------|
| [domain-model.md](ddd/domain-model.md) | Core domain concepts |
| [bounded-contexts.md](ddd/bounded-contexts.md) | Context boundaries |
| [aggregates.md](ddd/aggregates.md) | Aggregate roots |
| [entities.md](ddd/entities.md) | Entity definitions |
| [value-objects.md](ddd/value-objects.md) | Value object types |
| [domain-events.md](ddd/domain-events.md) | Event definitions |
| [repositories.md](ddd/repositories.md) | Repository interfaces |
| [services.md](ddd/services.md) | Domain services |

### SPARC Phases
| Phase | Document | Status |
|-------|----------|--------|
| 1 | [01-specification.md](sparc/01-specification.md) | Complete |
| 2 | [02-pseudocode.md](sparc/02-pseudocode.md) | Complete |
| 3 | [03-architecture.md](sparc/03-architecture.md) | Complete |
| 4 | [04-refinement.md](sparc/04-refinement.md) | Complete |
| 5 | [05-completion.md](sparc/05-completion.md) | Complete |

### Agent Configuration
| Document | Description |
|----------|-------------|
| [agent-roles.md](agents/agent-roles.md) | Agent type definitions |
| [task-decomposition.md](agents/task-decomposition.md) | Work breakdown |
| [coordination-protocol.md](agents/coordination-protocol.md) | Inter-agent comms |
| [research-agents.md](agents/research-agents.md) | Research specialist agents |

### Research
| Document | Description |
|----------|-------------|
| [training-techniques.md](research/training-techniques.md) | Training speed optimizations |
| [camera-trajectories.md](research/camera-trajectories.md) | Optimal camera placement |
| [360-vr-support.md](research/360-vr-support.md) | VR/360 considerations |
| [compression-streaming.md](research/compression-streaming.md) | Compression methods |
| [format-standards.md](research/format-standards.md) | File format specs |
| [rendering-advances.md](research/rendering-advances.md) | Rendering techniques |

---

## For Agentic Development

### Initialize Swarm
```bash
npx claude-flow@v3alpha swarm init --topology hierarchical-mesh --maxAgents 15
```

### Spawn Specialized Agents
```bash
# Research agents
npx claude-flow@v3alpha agent spawn -t researcher --name ARXIV_SCANNER --domain research

# Development agents
npx claude-flow@v3alpha agent spawn -t coder --name UE5_IMPLEMENTER --domain core
npx claude-flow@v3alpha agent spawn -t tester --name PIPELINE_TESTER --domain quality

# Architecture agents
npx claude-flow@v3alpha agent spawn -t system-architect --name EXPORT_ARCHITECT --domain design
```

### Task Assignment Workflow
```bash
# Create task
npx claude-flow@v3alpha task create -t feature -d "Implement spherical camera trajectory" --priority high

# Assign to agent
npx claude-flow@v3alpha task update --taskId <id> --assignTo UE5_IMPLEMENTER

# Monitor progress
npx claude-flow@v3alpha task status --taskId <id>
```

See [agent-roles.md](agents/agent-roles.md) for full agent specifications.

---

## Research Summary

### Key Findings

| Area | Finding | Impact |
|------|---------|--------|
| **Training Speed** | 3DGS-MCMC achieves 10-15x speedup (100-200s) | Core training integration |
| **Camera Views** | 100-180 views standard, 80 minimum viable | Capture trajectory design |
| **Output Formats** | PLY (universal), SPZ (~90% compression), glTF (web) | Export pipeline |
| **Platform Limits** | Desktop: 10M, Quest: 1M, Mobile: 500K splats | LOD system design |
| **Compression** | Vector quantization achieves 45x ratio | Streaming architecture |
| **Quality Target** | PSNR >27 dB acceptable for real-time | Training parameters |

### Training Method Comparison

| Method | Time | Speedup | PSNR |
|--------|------|---------|------|
| Original 3DGS | 35-45 min | 1x | 27.4 dB |
| 3DGS-MCMC | 100-200 sec | 10-15x | 27.6 dB |
| DashGaussian | 5 min | 7-9x | 27.3 dB |
| gsplat | 15-20 min | 2-3x | 27.5 dB |

### Compression Comparison

| Method | Ratio | Quality Loss |
|--------|-------|--------------|
| Quantized PLY | 3-4x | -0.1 dB |
| Vector Quantization | 45x | -0.3 dB |
| SPZ (Google) | ~10x | ~0 dB |
| HAC (Hierarchical) | 100x | -0.8 dB |

---

## Development Phases

### Phase 1: Core Export Pipeline (8 weeks)
- Scene Capture Module with trajectory generation
- COLMAP integration and processing
- Basic 3DGS training wrapper
- PLY export with validation

### Phase 2: Enhanced Features (6 weeks)
- Advanced compression (SPZ, VQ)
- LOD generation system
- Preview rendering in editor
- Quality metrics dashboard

### Phase 3: Production Hardening (4 weeks)
- Performance optimization
- Cross-platform testing
- Documentation and examples
- Marketplace preparation

See [05-completion.md](sparc/05-completion.md) for detailed roadmap.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    UE5 Editor                           │
├─────────────────────────────────────────────────────────┤
│  Scene Capture Module  │  Export Pipeline  │  Preview   │
├─────────────────────────────────────────────────────────┤
│              Processing Engine (Background)              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │
│  │ COLMAP   │  │ 3DGS     │  │ Compress │  │ LOD Gen │ │
│  │ Pipeline │→ │ Training │→ │ Pipeline │→ │ System  │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │
├─────────────────────────────────────────────────────────┤
│                Output Formats                           │
│         PLY  │  SPZ  │  glTF  │  Custom Binary         │
└─────────────────────────────────────────────────────────┘
```

---

## Contributing

1. Review [agent-roles.md](agents/agent-roles.md) for task ownership
2. Follow [coordination-protocol.md](agents/coordination-protocol.md) for workflow
3. Reference [glossary.md](specification/glossary.md) for terminology
4. Submit changes following ADR process

---

## References

- [3D Gaussian Splatting (Kerbl et al., SIGGRAPH 2023)](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/)
- [3DGS-MCMC (UBC Vision)](https://github.com/ubc-vision/3dgs-mcmc)
- [COLMAP](https://colmap.github.io/)
- [Unreal Engine Documentation](https://docs.unrealengine.com/)
