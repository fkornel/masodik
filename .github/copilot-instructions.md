# GitHub Copilot Instructions

This repository is the **SpecKit framework** — a meta-framework for AI-assisted, specification-driven software development. It contains no application source code; it provides the scaffolding, agents, prompts, and templates for a structured human+AI development workflow.

## Repository Purpose

SpecKit enforces a **spec-first development lifecycle** governed by a project constitution. AI agents guide each phase, producing progressively refined artifacts before any implementation begins.

## 9-Agent Workflow Pipeline

Agents are invoked via `/speckit.<name>` in GitHub Copilot Chat. They run in order:

| # | Agent | Inputs | Key Output |
|---|-------|--------|------------|
| 1 | `speckit.constitution` | Project principles | `.specify/memory/constitution.md` |
| 2 | `speckit.specify` | Feature description | `specs/[###-feature]/spec.md` |
| 3 | `speckit.clarify` | `spec.md` | Updated `spec.md` (max 5 questions) |
| 4 | `speckit.plan` | `spec.md` + constitution | `specs/[###-feature]/plan.md`, `data-model.md`, `contracts/`, `research.md` |
| 5 | `speckit.tasks` | `plan.md` + `spec.md` | `specs/[###-feature]/tasks.md` |
| 6 | `speckit.analyze` | `spec.md` + `plan.md` + `tasks.md` | Consistency report (read-only, no file changes) |
| 7 | `speckit.checklist` | `spec.md` | `specs/[###-feature]/checklists/*.md` |
| 8 | `speckit.taskstoissues` | `tasks.md` | GitHub Issues |
| 9 | `speckit.implement` | `tasks.md` | Executed code changes |

Each agent is defined in `.github/agents/speckit.*.agent.md` with a paired prompt in `.github/prompts/speckit.*.prompt.md`.

## Artifact Layout

```
specs/[###-feature]/
├── spec.md           # Feature specification (required)
├── plan.md           # Implementation plan (required before tasks)
├── research.md       # Technical decisions, POC results
├── data-model.md     # Entity/schema definitions
├── quickstart.md     # Integration examples
├── contracts/        # Interface contracts + test examples
├── tasks.md          # Executable task breakdown (required before implement)
└── checklists/       # Domain checklists (ux.md, security.md, test.md)
```

Feature artifacts live in `specs/[###-feature]/` where `###` is a zero-padded sequential number.

## Naming & Identifier Conventions

- **Branch names**: `[###-feature-name]` (e.g., `001-user-authentication`)
- **Functional Requirements**: `FR-###` (e.g., `FR-001`)
- **Success Criteria**: `SC-###` (e.g., `SC-001`)
- **User Story priority**: `P1` / `P2` / `P3` (P1 = must-have for MVP)
- **Task IDs**: `T###` (e.g., `T001`)
- **Checklist items**: `CHK###`
- **Normative language**: `**MUST**` / `**SHOULD**` in specs and plans
- **Unresolved decisions**: `[NEEDS CLARIFICATION]` placeholder in any artifact
- **Parallel tasks**: `[P]` marker in tasks.md — safe to run concurrently (different files, no deps)

### Task Checklist Format

```markdown
- [ ] T### [P] [US#] Short description with target file path
```

## Template Tokens

Templates in `.specify/templates/` use `[PLACEHOLDER]` syntax. Common tokens:

- `[PROJECT_NAME]`, `[FEATURE_NAME]`, `[###-feature-name]`
- `[PRINCIPLE_N_NAME]`, `[PRINCIPLE_N_DESCRIPTION]`

Do not leave `[PLACEHOLDER]` tokens or `NEEDS CLARIFICATION` markers in finalized artifacts.

## PowerShell Automation Scripts

Located in `.specify/scripts/powershell/`. These are invoked by agents automatically but can also be run manually:

```powershell
# Validate prerequisites for current feature branch
./.specify/scripts/powershell/check-prerequisites.ps1 -Json

# Initialize planning artifacts for current branch
./.specify/scripts/powershell/setup-plan.ps1 -Json

# Sync agent context with project tech stack
./.specify/scripts/powershell/update-agent-context.ps1 -AgentType copilot
```

`common.ps1` is a shared utility module sourced by the other scripts.

## Constitution

`.specify/memory/constitution.md` is the **governing document** for all feature work. Before modifying it:
- All changes require a version increment (semantic versioning)
- The amendment process defined in the constitution must be followed
- All agents read it as context; changes affect all future agent behavior

## Key Behaviors

- **`speckit.analyze`** is the only agent that makes zero file changes — it outputs a consistency report only.
- **`speckit.clarify`** asks at most 5 targeted questions and encodes answers back into `spec.md`.
- **`speckit.implement`** executes `tasks.md` phase-by-phase, respecting `[P]` parallelism and task dependencies. It marks tasks `[x]` as complete.
- All agents that produce artifacts follow the corresponding template in `.specify/templates/`.
