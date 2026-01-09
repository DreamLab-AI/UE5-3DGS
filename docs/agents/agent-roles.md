# UE5-3DGS Agent Role Definitions

## Overview

This document defines specialized agents for the UE5-3DGS project development. Each agent has specific capabilities, responsibilities, and coordination patterns within the multi-agent swarm.

---

## Research Domain Agents

### ARXIV_SCANNER

**Purpose:** Monitor and analyse emerging 3DGS research from arXiv

**Agent Type:** `researcher`

**Capabilities:**
- Academic paper search and retrieval
- Research synthesis and summarisation
- Citation tracking and impact analysis
- Trend identification in 3DGS domain

**Search Domains:**
- `arxiv.org/list/cs.CV` (Computer Vision)
- `arxiv.org/list/cs.GR` (Graphics)
- `arxiv.org/list/cs.LG` (Machine Learning)

**Monitoring Frequency:** Daily scan, weekly synthesis report

**Priority Papers:**
- 3DGS original (Kerbl et al., SIGGRAPH 2023)
- Mip-Splatting (Yu et al., 2024)
- 2D Gaussian Splatting (Huang et al., 2024)
- GaussianPro (Cheng et al., 2024)
- Scaffold-GS (Lu et al., 2024)
- 3DGS-DR (Ye et al., 2024)
- GOF (Yu et al., 2024)
- GS-LRM (Zhang et al., 2024)

**Spawn Command:**
```bash
npx claude-flow@v3alpha agent spawn \
  -t researcher \
  --name ARXIV_SCANNER \
  --capabilities "arxiv,papers,3dgs,computer-vision,graphics" \
  --domain research
```

**Hook Integration:**
```bash
# Before starting research task
npx claude-flow@v3alpha hooks pre-task \
  --task-id "R-001" \
  --description "Survey 3DGS training implementations"

# After completing analysis
npx claude-flow@v3alpha hooks post-task \
  --task-id "R-001" \
  --success true \
  --agent ARXIV_SCANNER

# Store findings in memory
npx claude-flow@v3alpha memory store \
  --namespace research-findings \
  --key "3dgs-implementation-survey" \
  --value "[JSON findings]"
```

---

### GITHUB_TRACKER

**Purpose:** Monitor reference implementations, forks, and tooling

**Agent Type:** `researcher`

**Capabilities:**
- Repository monitoring and analysis
- Code pattern identification
- Dependency tracking
- License compliance verification

**Target Repositories:**
- `graphdeco-inria/gaussian-splatting` (canonical)
- `graphdeco-inria/diff-gaussian-rasterization`
- `nerfstudio-project/gsplat`
- `antimatter15/splat`
- `huggingface/gaussian-splatting`
- `aras-p/UnityGaussianSplatting`

**Monitoring Frequency:** Bi-daily

**Spawn Command:**
```bash
npx claude-flow@v3alpha agent spawn \
  -t researcher \
  --name GITHUB_TRACKER \
  --capabilities "github,repositories,code,dependencies,licenses" \
  --domain research
```

**Search Queries:**
```
topic:gaussian-splatting language:cpp stars:>50
topic:3dgs topic:export
"colmap" "gaussian" "export" in:readme
"unreal" "gaussian" OR "3dgs" OR "splat"
"cuda" "gaussian" "splatting" "optimization"
```

---

### HUGGINGFACE_MONITOR

**Purpose:** Track pre-trained models, datasets, and Spaces demos

**Agent Type:** `researcher`

**Capabilities:**
- Model discovery and evaluation
- Dataset cataloguing
- Demo space monitoring
- Integration assessment

**Search Scope:**
- Models: `task:image-to-3d`, `task:depth-estimation`
- Datasets: `task:3d-reconstruction`, synthetic scene datasets
- Spaces: Interactive 3DGS demos and tools

**Monitoring Frequency:** Weekly

**Spawn Command:**
```bash
npx claude-flow@v3alpha agent spawn \
  -t researcher \
  --name HUGGINGFACE_MONITOR \
  --capabilities "huggingface,models,datasets,spaces,inference" \
  --domain research
```

---

### STANDARDS_TRACKER

**Purpose:** Monitor format specifications and interoperability standards

**Agent Type:** `researcher`

**Capabilities:**
- Format specification analysis
- Compatibility assessment
- Extension tracking
- Tool support evaluation

**Target Standards:**
- COLMAP database schema and text format
- PLY format variants for gaussian splats
- glTF extensions for point-based representations
- OpenXR and WebXR splat rendering proposals
- USD neural asset proposals

**Sources:**
- `colmap.github.io/format.html`
- `paulbourke.net/dataformats/ply/`
- `github.com/KhronosGroup/glTF`
- `github.com/PixarAnimationStudios/OpenUSD`

**Spawn Command:**
```bash
npx claude-flow@v3alpha agent spawn \
  -t researcher \
  --name STANDARDS_TRACKER \
  --capabilities "formats,specifications,colmap,ply,gltf,usd" \
  --domain research
```

---

## Development Domain Agents

### CPP_DEV

**Purpose:** UE5 C++ plugin implementation

**Agent Type:** `coder`

**Capabilities:**
- Unreal Engine 5 plugin development
- C++ implementation (modern C++17/20)
- Rendering pipeline integration
- Buffer extraction and processing
- Build system configuration (UBT)

**Specialisations:**
- Scene Capture Module (SCM)
- Data Extraction Module (DEM)
- Format Conversion Module (FCM)
- Editor integration (toolbar, modes)

**Code Standards:**
- Follow UE5 coding conventions
- Use UPROPERTY/UFUNCTION macros appropriately
- Implement async patterns for heavy operations
- Write unit tests for all public APIs

**Spawn Command:**
```bash
npx claude-flow@v3alpha agent spawn \
  -t coder \
  --name CPP_DEV \
  --capabilities "cpp,ue5,unreal-engine,rendering,plugin-development" \
  --domain development
```

**Hook Integration:**
```bash
# Before editing C++ files
npx claude-flow@v3alpha hooks pre-edit \
  --file-path "Source/UE5_3DGS/Private/SCM_CaptureOrchestrator.cpp" \
  --operation update

# After editing with pattern training
npx claude-flow@v3alpha hooks post-edit \
  --file "Source/UE5_3DGS/Private/SCM_CaptureOrchestrator.cpp" \
  --success true \
  --train-patterns
```

---

### CUDA_DEV

**Purpose:** GPU compute for optional training module

**Agent Type:** `coder`

**Capabilities:**
- CUDA kernel development
- GPU memory management
- CUDA-UE5 integration
- Performance optimisation
- Numerical precision handling

**Specialisations:**
- Training module (TRN)
- Adam optimiser implementation
- Tile-based rasterisation
- Gradient computation

**Hardware Requirements:**
- NVIDIA GPU with CUDA 11.8+ support
- Compute capability 7.0+

**Spawn Command:**
```bash
npx claude-flow@v3alpha agent spawn \
  -t coder \
  --name CUDA_DEV \
  --capabilities "cuda,gpu,training,optimization,numerical-computing" \
  --domain training
```

---

### TEST_DEV

**Purpose:** Automated testing and validation

**Agent Type:** `tester`

**Capabilities:**
- UE5 automation framework
- Unit test development
- Integration test design
- Performance benchmarking
- Validation metric computation

**Test Categories:**
- Coordinate conversion accuracy
- COLMAP format compliance
- PLY export validation
- Capture performance benchmarks
- Round-trip verification

**Spawn Command:**
```bash
npx claude-flow@v3alpha agent spawn \
  -t tester \
  --name TEST_DEV \
  --capabilities "testing,automation,validation,benchmarking,coverage" \
  --domain testing
```

**Test Execution:**
```bash
# Run unit tests
npx claude-flow@v3alpha hooks pre-command \
  --command "UE5Editor -run=AutomationCommandline -filter=UE5_3DGS"

# Validate COLMAP export
npx claude-flow@v3alpha hooks pre-command \
  --command "colmap database_creator --database_path output/database.db"
```

---

### DOC_DEV

**Purpose:** Documentation maintenance and generation

**Agent Type:** `coder`

**Capabilities:**
- API documentation (Doxygen)
- Tutorial writing
- Best practices documentation
- Troubleshooting guides
- Blueprint documentation

**Documentation Types:**
- C++ API reference
- Blueprint node documentation
- User tutorials (quickstart, advanced)
- Troubleshooting FAQ
- Architecture decision records

**Spawn Command:**
```bash
npx claude-flow@v3alpha agent spawn \
  -t coder \
  --name DOC_DEV \
  --capabilities "documentation,api-docs,tutorials,doxygen,markdown" \
  --domain documentation
```

---

### CONTENT_DEV

**Purpose:** Create reference test scenes and content

**Agent Type:** `coder`

**Capabilities:**
- UE5 level design
- Test scene creation
- Asset organisation
- Lighting setup for capture testing

**Responsibilities:**
- Create standardised test levels
- Design scenes with known geometry for validation
- Establish lighting configurations for reproducibility

**Spawn Command:**
```bash
npx claude-flow@v3alpha agent spawn \
  -t coder \
  --name CONTENT_DEV \
  --capabilities "ue5-content,level-design,testing-assets,lighting" \
  --domain testing
```

---

## Agent Coordination Matrix

| Agent | Coordinates With | Shared Memory Namespace |
|-------|------------------|------------------------|
| ARXIV_SCANNER | GITHUB_TRACKER, CPP_DEV | research-findings |
| GITHUB_TRACKER | ARXIV_SCANNER, STANDARDS_TRACKER | research-findings |
| HUGGINGFACE_MONITOR | ARXIV_SCANNER | research-findings |
| STANDARDS_TRACKER | CPP_DEV, DOC_DEV | architecture-decisions |
| CPP_DEV | TEST_DEV, CUDA_DEV, DOC_DEV | implementation-state |
| CUDA_DEV | CPP_DEV, TEST_DEV | implementation-state |
| TEST_DEV | CPP_DEV, CONTENT_DEV | test-results |
| DOC_DEV | CPP_DEV, STANDARDS_TRACKER | documentation |
| CONTENT_DEV | TEST_DEV | test-results |

---

## Full Swarm Spawn Sequence

```bash
#!/bin/bash
# Spawn complete UE5-3DGS development swarm

# Initialize swarm with hierarchical-mesh topology
npx claude-flow@v3alpha swarm init \
  --topology hierarchical-mesh \
  --max-agents 12 \
  --strategy adaptive

# Start session
npx claude-flow@v3alpha hooks session-start \
  --session-id "ue5-3dgs-$(date +%Y%m%d)"

# Research agents (parallel)
npx claude-flow@v3alpha agent spawn -t researcher --name ARXIV_SCANNER --capabilities "arxiv,papers,3dgs" &
npx claude-flow@v3alpha agent spawn -t researcher --name GITHUB_TRACKER --capabilities "github,repositories,code" &
npx claude-flow@v3alpha agent spawn -t researcher --name HUGGINGFACE_MONITOR --capabilities "huggingface,models" &
npx claude-flow@v3alpha agent spawn -t researcher --name STANDARDS_TRACKER --capabilities "formats,specifications" &
wait

# Development agents (parallel)
npx claude-flow@v3alpha agent spawn -t coder --name CPP_DEV --capabilities "cpp,ue5,rendering" &
npx claude-flow@v3alpha agent spawn -t coder --name CUDA_DEV --capabilities "cuda,gpu,training" &
npx claude-flow@v3alpha agent spawn -t tester --name TEST_DEV --capabilities "testing,validation" &
npx claude-flow@v3alpha agent spawn -t coder --name DOC_DEV --capabilities "documentation,api" &
npx claude-flow@v3alpha agent spawn -t coder --name CONTENT_DEV --capabilities "ue5-content,testing" &
wait

# Verify swarm status
npx claude-flow@v3alpha swarm status --verbose

echo "UE5-3DGS swarm initialized with $(npx claude-flow@v3alpha agent list --count) agents"
```
