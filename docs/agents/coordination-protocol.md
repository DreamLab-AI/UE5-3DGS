# UE5-3DGS Agent Coordination Protocol

## Overview

This document defines how agents coordinate within the UE5-3DGS swarm, including memory sharing patterns, hook triggers, and consensus mechanisms for architectural decisions.

---

## Memory Sharing Patterns

### Namespace Architecture

The swarm uses a hierarchical memory namespace structure for coordination:

```
memory/
├── research-findings/          # Research agent outputs
│   ├── arxiv-papers/
│   ├── github-repos/
│   ├── huggingface-resources/
│   └── format-specs/
├── architecture-decisions/     # ADRs and design choices
│   ├── adr-001-coordinate-system
│   ├── adr-002-cuda-integration
│   └── pending-decisions/
├── implementation-state/       # Development progress
│   ├── modules/
│   │   ├── scm/
│   │   ├── dem/
│   │   ├── fcm/
│   │   └── trn/
│   └── interfaces/
├── test-results/              # Test outcomes
│   ├── unit-tests/
│   ├── integration-tests/
│   └── benchmarks/
└── coordination-state/         # Swarm coordination
    ├── agent-status/
    ├── task-assignments/
    └── consensus-proposals/
```

### Memory Operations

**Storing Research Findings:**
```bash
# ARXIV_SCANNER stores paper analysis
npx claude-flow@v3alpha memory store \
  --namespace research-findings \
  --key "arxiv-papers/mip-splatting-2024" \
  --value '{
    "paper_id": "arxiv:2311.16493",
    "title": "Mip-Splatting: Alias-free 3D Gaussian Splatting",
    "relevance_score": 0.95,
    "key_contributions": [
      "3D smoothing filter",
      "2D Mip filter",
      "Anti-aliasing for 3DGS"
    ],
    "applicable_components": ["rendering", "training"],
    "integration_complexity": "medium"
  }' \
  --metadata '{"agent": "ARXIV_SCANNER", "timestamp": "2025-01-09T12:00:00Z"}'
```

**Retrieving Implementation State:**
```bash
# CPP_DEV retrieves current module state
npx claude-flow@v3alpha memory retrieve \
  --namespace implementation-state \
  --key "modules/dem/status"
```

**Searching for Relevant Patterns:**
```bash
# Search for coordinate-related findings
npx claude-flow@v3alpha memory search \
  --query "coordinate conversion COLMAP"
  --namespace research-findings \
  --limit 10
```

### Memory Sharing Triggers

| Event | Source Agent | Target Namespace | Notification |
|-------|--------------|------------------|--------------|
| Paper found | ARXIV_SCANNER | research-findings | All researchers |
| Repo updated | GITHUB_TRACKER | research-findings | CPP_DEV, CUDA_DEV |
| ADR proposed | Any | architecture-decisions | All agents |
| Code committed | CPP_DEV | implementation-state | TEST_DEV |
| Test completed | TEST_DEV | test-results | CPP_DEV, DOC_DEV |
| Task started | Any | coordination-state | Coordinator |
| Task completed | Any | coordination-state | Dependent agents |

---

## Hook Triggers

### Pre-Task Hook

Triggered when an agent begins a task assignment.

```bash
# Usage
npx claude-flow@v3alpha hooks pre-task \
  --task-id "D-002" \
  --description "Implement coordinate conversion utilities"
```

**Actions Performed:**
1. Validate task dependencies are complete
2. Check memory for relevant context
3. Notify dependent agents
4. Record task start time
5. Return agent suggestions for the task

**Response Schema:**
```json
{
  "task_id": "D-002",
  "status": "ready",
  "dependencies_met": true,
  "relevant_memory": [
    {
      "key": "research-findings/coordinate-benchmarks",
      "relevance": 0.95
    }
  ],
  "suggested_agents": ["CPP_DEV"],
  "estimated_duration_hours": 8
}
```

### Post-Task Hook

Triggered when an agent completes a task.

```bash
# Usage
npx claude-flow@v3alpha hooks post-task \
  --task-id "D-002" \
  --success true \
  --agent "CPP_DEV" \
  --quality 0.95
```

**Actions Performed:**
1. Record task completion
2. Store task results in memory
3. Notify dependent tasks
4. Train neural patterns from outcome
5. Update coordination state

### Pre-Edit Hook

Triggered before modifying a file.

```bash
# Usage
npx claude-flow@v3alpha hooks pre-edit \
  --file-path "Source/UE5_3DGS/Private/DEM_CoordinateConverter.cpp" \
  --operation "update"
```

**Actions Performed:**
1. Check file ownership (which agent last edited)
2. Validate coding standards
3. Load relevant context from memory
4. Suggest related files to consider

**Response Schema:**
```json
{
  "file": "DEM_CoordinateConverter.cpp",
  "last_editor": "CPP_DEV",
  "related_files": [
    "DEM_CoordinateConverter.h",
    "DEM_CameraParameters.h"
  ],
  "relevant_context": {
    "adr": "adr-001-coordinate-system",
    "test_file": "CoordinateConversionTest.cpp"
  },
  "warnings": []
}
```

### Post-Edit Hook

Triggered after modifying a file.

```bash
# Usage
npx claude-flow@v3alpha hooks post-edit \
  --file "Source/UE5_3DGS/Private/DEM_CoordinateConverter.cpp" \
  --success true \
  --train-patterns
```

**Actions Performed:**
1. Run relevant unit tests
2. Update documentation index
3. Train neural patterns from edit
4. Notify interested agents
5. Record edit in memory

### Pre-Command Hook

Triggered before executing a command.

```bash
# Usage
npx claude-flow@v3alpha hooks pre-command \
  --command "UE5Editor -run=AutomationCommandline -filter=UE5_3DGS"
```

**Actions Performed:**
1. Assess command risk level
2. Validate command parameters
3. Check resource availability
4. Return approval or rejection

### Post-Command Hook

Triggered after command execution.

```bash
# Usage
npx claude-flow@v3alpha hooks post-command \
  --command "UE5Editor -run=AutomationCommandline" \
  --exit-code 0
```

**Actions Performed:**
1. Record command outcome
2. Parse and store results
3. Trigger dependent workflows
4. Update metrics

---

## Consensus Mechanism for Architectural Decisions

### Architecture Decision Records (ADRs)

Major architectural decisions require swarm consensus. The process follows:

**1. Proposal Phase**

Any agent can propose an ADR:

```bash
npx claude-flow@v3alpha hive-mind consensus \
  --action propose \
  --type "architecture-decision" \
  --value '{
    "adr_id": "ADR-003",
    "title": "CUDA Context Management Strategy",
    "status": "proposed",
    "context": "Need to decide how CUDA context integrates with UE5 plugin lifecycle",
    "options": [
      {
        "id": "A",
        "title": "Lazy Initialization",
        "description": "Create CUDA context on first training request",
        "pros": ["No overhead if training unused", "Simpler lifecycle"],
        "cons": ["First-use latency", "Error handling complexity"]
      },
      {
        "id": "B",
        "title": "Plugin Load Initialization",
        "description": "Create CUDA context when plugin loads",
        "pros": ["Deterministic timing", "Early error detection"],
        "cons": ["Resource usage even if unused", "Slower plugin load"]
      }
    ],
    "recommendation": "A",
    "proposer": "CUDA_DEV",
    "reviewers": ["CPP_DEV", "TEST_DEV"]
  }'
```

**2. Voting Phase**

Designated reviewers vote on the proposal:

```bash
# CPP_DEV votes
npx claude-flow@v3alpha hive-mind consensus \
  --action vote \
  --proposal-id "ADR-003" \
  --voter-id "CPP_DEV" \
  --vote true

# TEST_DEV votes
npx claude-flow@v3alpha hive-mind consensus \
  --action vote \
  --proposal-id "ADR-003" \
  --voter-id "TEST_DEV" \
  --vote true
```

**3. Resolution Phase**

Once quorum is reached:

```bash
# Check consensus status
npx claude-flow@v3alpha hive-mind consensus \
  --action status \
  --proposal-id "ADR-003"
```

**Response:**
```json
{
  "proposal_id": "ADR-003",
  "status": "approved",
  "votes_for": 3,
  "votes_against": 0,
  "quorum_met": true,
  "decided_option": "A",
  "decision_timestamp": "2025-01-09T15:30:00Z"
}
```

**4. Storage Phase**

Approved ADRs are stored in memory:

```bash
npx claude-flow@v3alpha memory store \
  --namespace architecture-decisions \
  --key "adr-003-cuda-context" \
  --value '[ADR content]'
```

### Consensus Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| Quorum Size | 3 | Minimum votes required |
| Consensus Type | Byzantine | Tolerates f < n/3 faulty |
| Voting Period | 24 hours | Maximum time for voting |
| Tie-Break Rule | Proposer decides | On equal votes |

### Decision Categories Requiring Consensus

| Category | Quorum | Reviewers |
|----------|--------|-----------|
| API Design | 3 | CPP_DEV, TEST_DEV, DOC_DEV |
| Format Specification | 3 | STANDARDS_TRACKER, CPP_DEV, TEST_DEV |
| Performance Trade-off | 3 | CPP_DEV, CUDA_DEV, TEST_DEV |
| External Dependency | 4 | All development agents |
| Security Decision | 4 | All development agents |

---

## Inter-Agent Communication Patterns

### Direct Notification

For urgent or targeted communication:

```bash
npx claude-flow@v3alpha hooks notify \
  --target "CPP_DEV" \
  --message "COLMAP format spec updated - review R-002 findings" \
  --priority high \
  --data '{"task_id": "R-002", "update_type": "format_change"}'
```

### Broadcast Notification

For swarm-wide announcements:

```bash
npx claude-flow@v3alpha hive-mind broadcast \
  --message "Phase 1 milestone achieved - COLMAP export functional" \
  --priority normal \
  --from-id "coordinator"
```

### Task Routing

Route tasks to optimal agents based on learned patterns:

```bash
npx claude-flow@v3alpha hooks route \
  --task "Implement quaternion conversion for COLMAP compatibility" \
  --context "Requires deep understanding of coordinate systems"
```

**Response:**
```json
{
  "recommended_agent": "CPP_DEV",
  "confidence": 0.92,
  "reasoning": "High success rate with coordinate conversion tasks",
  "alternatives": [
    {"agent": "CUDA_DEV", "confidence": 0.65}
  ]
}
```

---

## Session Management

### Session Start

Initialize coordination state at session start:

```bash
npx claude-flow@v3alpha hooks session-start \
  --session-id "ue5-3dgs-phase1-$(date +%Y%m%d)" \
  --start-daemon true
```

**Actions:**
1. Start background worker daemon
2. Load previous session state (if restoring)
3. Initialize agent pool
4. Set up memory namespaces
5. Configure hook triggers

### Session End

Persist state and cleanup:

```bash
npx claude-flow@v3alpha hooks session-end \
  --save-state true \
  --export-metrics true \
  --stop-daemon true
```

**Actions:**
1. Save current swarm state
2. Export session metrics
3. Consolidate memory
4. Stop background workers
5. Generate session summary

### Session Restore

Resume from previous session:

```bash
npx claude-flow@v3alpha hooks session-restore \
  --session-id "latest" \
  --restore-agents true \
  --restore-tasks true
```

---

## Coordination Workflow Examples

### Example 1: Research-to-Development Handoff

```bash
# 1. ARXIV_SCANNER completes research task
npx claude-flow@v3alpha hooks post-task \
  --task-id "R-003" \
  --success true \
  --agent "ARXIV_SCANNER"

# 2. Store findings in shared memory
npx claude-flow@v3alpha memory store \
  --namespace research-findings \
  --key "coordinate-conversion-benchmarks" \
  --value '[benchmark results]'

# 3. Notify dependent development task
npx claude-flow@v3alpha hooks notify \
  --target "CPP_DEV" \
  --message "R-003 complete - D-002 dependencies met" \
  --priority high

# 4. CPP_DEV retrieves context and starts task
npx claude-flow@v3alpha memory retrieve \
  --namespace research-findings \
  --key "coordinate-conversion-benchmarks"

npx claude-flow@v3alpha hooks pre-task \
  --task-id "D-002" \
  --description "Implement coordinate conversion utilities"
```

### Example 2: Code Review Coordination

```bash
# 1. CPP_DEV completes implementation
npx claude-flow@v3alpha hooks post-edit \
  --file "Source/UE5_3DGS/Private/FCM_COLMAPWriter.cpp" \
  --success true

# 2. TEST_DEV is notified and runs validation
npx claude-flow@v3alpha hooks pre-command \
  --command "colmap model_converter --input_path output/ --output_path validation/"

# 3. Results stored for review
npx claude-flow@v3alpha memory store \
  --namespace test-results \
  --key "colmap-export-validation" \
  --value '{"valid": true, "warnings": []}'

# 4. DOC_DEV updates documentation
npx claude-flow@v3alpha hooks pre-edit \
  --file-path "docs/api/FCM_COLMAPWriter.md" \
  --operation "update"
```

### Example 3: Architectural Decision Process

```bash
# 1. CPP_DEV identifies need for decision
npx claude-flow@v3alpha hive-mind consensus \
  --action propose \
  --type "architecture-decision" \
  --value '{"adr_id": "ADR-004", "title": "Depth Buffer Format"}'

# 2. Broadcast to reviewers
npx claude-flow@v3alpha hive-mind broadcast \
  --message "ADR-004 proposed: Depth Buffer Format - votes needed" \
  --priority high

# 3. Collect votes
npx claude-flow@v3alpha hive-mind consensus \
  --action vote --proposal-id "ADR-004" --voter-id "TEST_DEV" --vote true

npx claude-flow@v3alpha hive-mind consensus \
  --action vote --proposal-id "ADR-004" --voter-id "CUDA_DEV" --vote true

# 4. Check status and proceed
npx claude-flow@v3alpha hive-mind consensus \
  --action status --proposal-id "ADR-004"

# 5. Store approved decision
npx claude-flow@v3alpha memory store \
  --namespace architecture-decisions \
  --key "adr-004-depth-buffer-format" \
  --value '[approved ADR]'
```

---

## Monitoring and Metrics

### View Coordination Metrics

```bash
npx claude-flow@v3alpha hooks metrics \
  --period 24h \
  --include-v3 true
```

**Output:**
```json
{
  "period": "24h",
  "tasks_completed": 12,
  "tasks_failed": 1,
  "avg_task_duration_hours": 4.2,
  "memory_operations": 156,
  "consensus_proposals": 2,
  "consensus_approved": 2,
  "agent_utilization": {
    "CPP_DEV": 0.85,
    "TEST_DEV": 0.72,
    "ARXIV_SCANNER": 0.45
  },
  "neural_patterns_trained": 23
}
```

### Check Agent Health

```bash
npx claude-flow@v3alpha agent health \
  --threshold 0.8
```

### View Swarm Status

```bash
npx claude-flow@v3alpha swarm status \
  --verbose
```
