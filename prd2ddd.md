---
name: prd-to-docs
description: Generate complete ADR, DDD, and SPARC documentation from a Product Requirements Document (PRD)
arguments:
  - name: prd_input
    description: Path to PRD file or inline PRD content
    required: true
---

# PRD to Documentation Workflow

You are executing a comprehensive documentation generation workflow from a Product Requirements Document (PRD).

## Input PRD

$ARGUMENTS

## Workflow Execution

Execute the following phases in order, generating all documentation in the `docs/` directory structure.

### Phase 1: PRD Analysis & Specification

**Objective**: Extract and document all requirements from the PRD.

1. Read and analyze the provided PRD thoroughly
2. Create `docs/specification/` directory with:
   - `requirements.md` - Functional requirements extracted from PRD
   - `non-functional.md` - NFRs (performance, security, scalability)
   - `edge-cases.md` - Edge cases and boundary conditions
   - `constraints.md` - Technical and business constraints
   - `glossary.md` - Domain terminology definitions

### Phase 2: DDD Documentation

**Objective**: Model the domain using Domain-Driven Design patterns.

1. Identify bounded contexts from the requirements
2. Create `docs/ddd/` directory with:
   - `domain-model.md` - Core domain model overview
   - `bounded-contexts.md` - Context boundaries and relationships
   - `aggregates.md` - Aggregate roots and consistency boundaries
   - `entities.md` - Domain entities with identity
   - `value-objects.md` - Immutable value objects
   - `domain-events.md` - Event catalog with triggers and handlers
   - `repositories.md` - Repository interfaces
   - `services.md` - Domain and application services

### Phase 3: ADR Generation

**Objective**: Document architectural decisions with rationale.

1. Identify key architectural decisions from the domain model
2. Create `docs/adr/` directory with ADR files using this template:

```markdown
# ADR-XXX: [Decision Title]

## Status
[Proposed | Accepted | Deprecated | Superseded]

## Context
[What is the issue we're addressing?]

## Decision
[What is the change we're proposing?]

## Consequences
### Positive
- [Benefits]

### Negative
- [Trade-offs]

## Alternatives Considered
1. [Alternative approaches evaluated]
```

3. Generate ADRs for:
   - Architecture style (monolith/microservices/modular)
   - Database technology choices
   - API design approach
   - Authentication/authorization strategy
   - Deployment architecture
   - Any other significant technical decisions

4. Create `docs/adr/index.md` - ADR index with links to all decisions

### Phase 4: SPARC Phase Documentation

**Objective**: Generate implementation-ready SPARC documentation.

1. Create `docs/sparc/` directory with:
   - `01-specification.md` - Detailed specifications with acceptance criteria
   - `02-pseudocode.md` - High-level algorithms and flow logic
   - `03-architecture.md` - System architecture with diagrams
   - `04-refinement.md` - TDD anchors and test specifications
   - `05-completion.md` - Integration checklist and deployment guide

### Phase 5: Summary Generation

1. Create `docs/README.md` with:
   - Overview of generated documentation
   - Navigation guide to all sections
   - Quick start for developers
   - Links to all major documents

## Output Structure

```
docs/
├── README.md                     # Documentation overview
├── specification/
│   ├── requirements.md
│   ├── non-functional.md
│   ├── edge-cases.md
│   ├── constraints.md
│   └── glossary.md
├── ddd/
│   ├── domain-model.md
│   ├── bounded-contexts.md
│   ├── aggregates.md
│   ├── entities.md
│   ├── value-objects.md
│   ├── domain-events.md
│   ├── repositories.md
│   └── services.md
├── adr/
│   ├── index.md
│   ├── ADR-001-*.md
│   ├── ADR-002-*.md
│   └── ...
└── sparc/
    ├── 01-specification.md
    ├── 02-pseudocode.md
    ├── 03-architecture.md
    ├── 04-refinement.md
    └── 05-completion.md
```

## Execution Rules

1. **Modularity**: Keep each file under 500 lines
2. **No Secrets**: Never include hardcoded credentials or API keys
3. **Cross-References**: Link related documents using relative paths
4. **Diagrams**: Include Mermaid diagrams where helpful
5. **TDD Anchors**: Include testable acceptance criteria in specifications
6. **Traceability**: Reference PRD sections in requirements

## Begin Workflow

Start by reading/analyzing the PRD input, then execute each phase sequentially. Create all directories and files as specified. Provide a summary upon completion listing all generated documents.
