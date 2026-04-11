---

description: "Task list for Manual Model Split Serving"
---

# Tasks: Manual Model Split Serving

**Input**: Design documents from `/specs/002-manual-model-split/`  
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/, quickstart.md

**Tests**: Automated unit, integration, smoke, es benchmark feladatok kotelezoek, mert a feature kozvetlenul erinti a modellbetoltest, a device-hozzarendelest, a memoria-viselkedest, a latency-t, es a stabilitast.

**Organization**: A taskok user story-k szerint vannak csoportositva, hogy a split startup, a hosszu kontextusu stabilitas, es a stage-diagnosztika kulon is szallithato es validalhato legyen.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Parhuzamosan vegezheto, ha mas fajlokat erint es nincs kozvetlen blokkolo fuggese
- **[Story]**: Az adott user storyhoz tartozo feladat (US1, US2, US3)
- Minden feladat konkret fajlutvonalat tartalmaz

## Path Conventions

- **Native C++ project**: `include/`, `src/`, `tests/`, `benchmarks/` a repository gyokerben
- **Serving/runtime modules**: `include/masodik/genai/`, `include/masodik/runtime/`, `include/masodik/config/`, `include/masodik/diagnostics/`, valamint a megfelelo `src/` alkonyvtarak
- **Feature docs**: `specs/002-manual-model-split/`
- **Smoke and validation scripts**: `tests/smoke/` es `tools/scripts/`

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: A manual split featurehez szukseges fixture-ok, profilvaz es futtatoi entrypointok elokeszitese

- [x] T001 Create split feature benchmark fixture payload in benchmarks/fixtures/manual-split-long-context-request.json
- [x] T002 [P] Create split startup smoke script skeleton in tests/smoke/test_manual_split_startup.ps1
- [x] T003 [P] Create split long-context smoke script skeleton in tests/smoke/test_manual_split_long_context.ps1
- [x] T004 [P] Create split diagnostics smoke script skeleton in tests/smoke/test_manual_split_diagnostics.ps1
- [x] T005 Register split smoke script entry points in tools/scripts/run-smoke-suite.ps1 and tools/scripts/validate-quickstart.ps1
- [x] T006 Register split benchmark placeholders in benchmarks/CMakeLists.txt and tools/scripts/run-benchmarks.ps1

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: A split artifact, a ket-stage pipeline, es a kozos health/diagnosztikai primitivek megteremtese

**CRITICAL**: Erre a fazisra epul minden user story

- [x] T007 Extend split-capable serving profile fields in include/masodik/config/serving_profile.hpp and src/config/serving_profile.cpp
- [x] T008 [P] Add split profile generation config projection in include/masodik/runtime/service_config.hpp, src/runtime/service_config.cpp, and include/masodik/genai/generation_config.hpp
- [x] T009 [P] Create split artifact manifest DTO and loader in include/masodik/config/model_repository.hpp and src/config/model_repository.cpp
- [x] T010 [P] Create split runtime entity types for execution stages and handoff state in include/masodik/genai/llm_pipeline_service.hpp
- [x] T011 Add split health snapshot fields in include/masodik/runtime/health_snapshot.hpp and include/masodik/runtime/readiness_reporter.hpp
- [x] T012 [P] Add split health/model payload serialization fields in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T013 [P] Add split error categories and failure scopes in include/masodik/runtime/error_category.hpp, src/runtime/error_category.cpp, and include/masodik/server/openai_error_mapper.hpp
- [x] T014 Create split-specific startup validation hooks in include/masodik/runtime/startup_validation.hpp, src/runtime/startup_validation.cpp, include/masodik/runtime/service_dependency_validator.hpp, and src/runtime/service_dependency_validator.cpp
- [x] T015 Create split-aware queue ownership primitives for per-request handoff isolation in include/masodik/runtime/request_scheduler.hpp and src/runtime/request_scheduler.cpp
- [x] T016 Create split diagnostics logging primitives in include/masodik/diagnostics/logger.hpp, src/diagnostics/logger.cpp, include/masodik/diagnostics/runtime_error_reporter.hpp, and src/diagnostics/runtime_error_reporter.cpp
- [x] T017 Add split profile baseline configuration file in config/profiles/b580-manual-split.json and config/profiles/local-b580.example.json
- [x] T018 Document split artifact storage and profile expectations in docs/local-startup.md and docs/troubleshooting.md

**Checkpoint**: A repository kepes split artifactet, split profilt, es split health/diagnosztikai alapokat kezelni.

---

## Phase 3: User Story 1 - Ketlepcsos modellbetoltes (Priority: P1) MVP

**Goal**: A szolgaltatas explicit ket-stage modellartifactbol induljon, a dGPU es iGPU stage-eket kulon betoltse, es csak teljes split readiness eseten lepjen ready allapotba.

**Independent Test**: A split profillal inditott szolgaltatas ervenyes ketreszes artifact mellett ready lesz, stage-1 es stage-2 aktiv allapotot jelent, es hianyos vagy inkompatibilis masodik stage eseten nem marad ready.

### Tests for User Story 1

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T019 [P] [US1] Add split manifest parsing and validation unit coverage in tests/unit/test_model_repository_split_manifest.cpp
- [x] T020 [P] [US1] Add split serving profile loading coverage in tests/unit/test_serving_profile_split.cpp
- [x] T021 [P] [US1] Add split startup readiness integration coverage in tests/integration/test_manual_split_startup.cpp
- [x] T022 [P] [US1] Add split startup failure integration coverage for missing or incompatible stage artifacts in tests/integration/test_manual_split_startup_failures.cpp
- [x] T023 [P] [US1] Add split cold-start benchmark in benchmarks/cold-start/benchmark_manual_split_cold_start.cpp
- [x] T024 [P] [US1] Implement split startup smoke validation in tests/smoke/test_manual_split_startup.ps1

### Implementation for User Story 1

- [x] T025 [P] [US1] Add split artifact manifest validation logic in include/masodik/config/model_repository.hpp and src/config/model_repository.cpp
- [x] T026 [P] [US1] Add split profile-to-runtime mapping in include/masodik/runtime/service_config.hpp and src/runtime/service_config.cpp
- [x] T027 [P] [US1] Create split stage loader abstractions in include/masodik/genai/llm_pipeline_service.hpp and src/genai/llm_pipeline_service.cpp
- [x] T028 [US1] Implement stage-1 compiled model initialization and warmup path in src/genai/llm_pipeline_service.cpp
- [x] T029 [US1] Implement stage-2 compiled model initialization and warmup path in src/genai/llm_pipeline_service.cpp
- [x] T030 [US1] Enforce two-device startup gating and reject same-device or CPU fallback in src/genai/llm_pipeline_service.cpp and src/runtime/startup_validation.cpp
- [x] T031 [US1] Surface split readiness fields through health and launcher reporting in src/runtime/readiness_reporter.cpp, src/server/openai_handlers.cpp, and src/main/launcher_app.cpp
- [x] T032 [US1] Add Hungarian startup rationale and profile notes in config/profiles/b580-manual-split.json and docs/local-startup.md

**Checkpoint**: A manual split startup, a ket-stage betoltes, es a strict ready/fallback logika onalloan mukodik.

---

## Phase 4: User Story 2 - Stabil hosszu kontextusu kiszolgalas (Priority: P2)

**Goal**: A split profil bounded memoria mellett kezeljen nagy kontextusu kerest, a handoff explicit legyen, es tulterheles eseten szabalyos hibaval alljon meg.

**Independent Test**: Egy reprezentativ long-context keres a split profilon teljesen vegigfut, valaszt ad, es a rendszer nem lep unhealthy allapotba memoriafelfutas miatt; memoria-tullepesnel szabalyos hiba es fallback-tiltas latszik.

### Tests for User Story 2

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T033 [P] [US2] Add handoff lifecycle unit coverage in tests/unit/test_manual_split_handoff.cpp
- [x] T034 [P] [US2] Add split long-context integration coverage in tests/integration/test_manual_split_long_context.cpp
- [x] T035 [P] [US2] Add split memory budget failure integration coverage in tests/integration/test_manual_split_memory_budget.cpp
- [x] T036 [P] [US2] Add split long-context stability benchmark in benchmarks/queue-soak/benchmark_manual_split_long_context.cpp
- [x] T037 [P] [US2] Add split handoff throughput benchmark in benchmarks/steady-state/benchmark_manual_split_handoff_latency.cpp
- [x] T038 [P] [US2] Implement split long-context smoke validation in tests/smoke/test_manual_split_long_context.ps1

### Implementation for User Story 2

- [x] T039 [P] [US2] Implement per-request handoff object creation and cleanup in include/masodik/genai/llm_pipeline_service.hpp, src/genai/llm_pipeline_service.cpp, include/masodik/runtime/request_scheduler.hpp, and src/runtime/request_scheduler.cpp
- [x] T040 [P] [US2] Implement hidden-state transfer and stage chaining flow in src/genai/llm_pipeline_service.cpp
- [x] T041 [P] [US2] Add split memory budget calculation and enforcement in include/masodik/genai/generation_config.hpp, src/genai/llm_pipeline_service.cpp, and src/runtime/request_timeout_policy.cpp
- [x] T042 [US2] Add bounded long-context guardrails and overload rejection paths in src/genai/llm_pipeline_service.cpp and src/server/openai_error_mapper.cpp
- [x] T043 [US2] Tune split queue and concurrency defaults for low-concurrency MVP in config/profiles/b580-manual-split.json and src/runtime/queue_policy.cpp
- [x] T044 [US2] Extend launcher and runtime logs with long-context and memory guard events in src/main/launcher_app.cpp and src/diagnostics/runtime_error_reporter.cpp
- [x] T045 [US2] Document long-context operating limits and failure handling in docs/troubleshooting.md and specs/002-manual-model-split/quickstart.md

**Checkpoint**: A split serving mar hosszu kontextusu kerest is kezel bounded memoria es szabalyos tulterhelesi viselkedes mellett.

---

## Phase 5: User Story 3 - Attekintheto stage-diagnosztika (Priority: P3)

**Goal**: A split szolgaltatas kulon lathato stage- es handoff-allapotot, hiba-scope-ot, es memoria-osszefoglalot adjon healthben es naplokban.

**Independent Test**: Stage-1, stage-2, vagy handoff hiba szandekos eloidezese eseten a health payload es a runtime log ugyanazt a hiba-scope-ot jelzi, es a rendszer nem hazudik ready allapotot.

### Tests for User Story 3

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T046 [P] [US3] Add split health payload serialization unit coverage in tests/unit/test_manual_split_health_payload.cpp
- [x] T047 [P] [US3] Add split runtime failure scope integration coverage in tests/integration/test_manual_split_failure_scope.cpp
- [x] T048 [P] [US3] Add split degraded-health integration coverage in tests/integration/test_manual_split_health_degradation.cpp
- [x] T049 [P] [US3] Add split diagnostics smoke validation in tests/smoke/test_manual_split_diagnostics.ps1
- [x] T050 [P] [US3] Add split diagnostics benchmark correlation scenario in benchmarks/steady-state/benchmark_manual_split_diagnostics_overhead.cpp

### Implementation for User Story 3

- [x] T051 [P] [US3] Implement stage diagnostic snapshot assembly in include/masodik/runtime/health_snapshot.hpp, src/runtime/readiness_reporter.cpp, and src/genai/llm_pipeline_service.cpp
- [x] T052 [P] [US3] Extend `/health` and status payloads with split stage fields in include/masodik/api/openai_contract.hpp, src/api/openai_contract.cpp, and src/server/openai_handlers.cpp
- [x] T053 [P] [US3] Classify startup, stage, and handoff failure scopes in src/genai/llm_pipeline_service.cpp, include/masodik/runtime/error_category.hpp, and src/runtime/error_category.cpp
- [x] T054 [US3] Emit structured split stage log lines and last-failure summaries in include/masodik/diagnostics/logger.hpp, src/diagnostics/logger.cpp, and src/diagnostics/runtime_error_reporter.cpp
- [x] T055 [US3] Ensure split runtime failures force non-ready health semantics in src/genai/llm_pipeline_service.cpp and src/runtime/readiness_reporter.cpp
- [x] T056 [US3] Add Hungarian diagnostics operating notes in docs/diagnostics-contract.md, docs/troubleshooting.md, and specs/002-manual-model-split/quickstart.md

**Checkpoint**: A split szolgaltatas stage-szinten diagnosztizalhato, a health oszinte, es a hibaterulet gyorsan beazonosithato.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Regresszioellenorzes, dokumentacios zaras, es benchmark baseline rogzitese minden story felett

- [x] T057 [P] Add split contract-to-code traceability notes in specs/002-manual-model-split/contracts/split-artifact-manifest.md, specs/002-manual-model-split/contracts/split-health-contract.md, and docs/diagnostics-contract.md
- [x] T058 [P] Add split quickstart validation steps to tools/scripts/validate-quickstart.ps1 and tools/scripts/run-smoke-suite.ps1
- [x] T059 [P] Add split profile matrix coverage in tests/smoke/test_profile_matrix.ps1 and tests/smoke/test_no_silent_degradation.ps1
- [x] T060 Add split benchmark baseline export support in tools/profiling/export-benchmark-results.ps1 and docs/performance-baseline.md
- [x] T061 [P] Add split regression aggregation target in tests/CMakeLists.txt, benchmarks/CMakeLists.txt, and tools/scripts/build-and-test.ps1
- [x] T062 Add final Hungarian docs and README updates for manual split serving in README.md, docs/local-startup.md, and docs/performance-baseline.md

---

## Phase 7: User Story 1 - Valodi split artifact generalas (Priority: P1)

**Goal**: A szolgaltatas a helyi monolitikus `gpt-oss-20b-int4-ov` modellbol repeatable modon eloallitott, ketreszes split artifactbol induljon, amely rogzitett reteghatarral, checksum-mal, es stage-1/stage-2 metadata-val rendelkezik.

**Independent Test**: A split artifact generator a helyi AI Playground modellrootbol eloallit egy `split-manifest.json`-os artifact gyokeret, amely Stage-1 = kb. `0-17`, Stage-2 = kb. `18-43` retegtartomanyt ir le, es a szolgaltatas ezzel CPU fallback nelkul startup-ready allapotba kerul.

### Tests for User Story 1

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T063 [P] [US1] Add split artifact builder unit coverage in tests/unit/test_split_artifact_builder.cpp
- [x] T064 [P] [US1] Add generated split-manifest integration coverage in tests/integration/test_manual_split_generated_artifact.cpp
- [x] T065 [P] [US1] Add offline split artifact smoke validation in tests/smoke/test_generate_manual_split_artifact.ps1
- [x] T066 [P] [US1] Add generated-artifact cold-start benchmark scenario in benchmarks/cold-start/benchmark_manual_split_generated_artifact.cpp

### Implementation for User Story 1

- [x] T067 [P] [US1] Create split artifact builder types in include/masodik/split/split_artifact_builder.hpp and src/split/split_artifact_builder.cpp
- [x] T068 [P] [US1] Create split artifact export DTOs in include/masodik/split/split_artifact_manifest_writer.hpp and src/split/split_artifact_manifest_writer.cpp
- [x] T069 [US1] Implement monolithic gpt-oss-20b boundary discovery and source checksum capture in src/split/split_artifact_builder.cpp
- [x] T070 [US1] Implement Stage-1 and Stage-2 IR export plus `split-manifest.json` emission in src/split/split_artifact_builder.cpp and src/split/split_artifact_manifest_writer.cpp
- [x] T071 [US1] Add split artifact generator CLI entrypoint in src/main/split_artifact_tool.cpp and src/main/CMakeLists.txt
- [x] T072 [US1] Add repeatable PowerShell wrapper for split artifact generation in tools/scripts/generate-manual-split-artifact.ps1
- [x] T073 [US1] Extend manifest validation for `source_model_checksum`, `cut_after_layer`, `layer_range`, and `validated_context_tokens` in include/masodik/config/model_repository.hpp and src/config/model_repository.cpp
- [x] T074 [US1] Document generated split artifact workflow and target 18/26 layer split in docs/local-startup.md and specs/002-manual-model-split/quickstart.md

**Checkpoint**: A feature mar nem csak split-aware startupot tud, hanem valodi, reprodukalhato split artifactot allit elo es fogyaszt.

---

## Phase 8: User Story 2 - Compiled ketstage-es infer ut es 128k validacio (Priority: P2)

**Goal**: A manual split request-ut a placeholder valasz helyett tenyleges Stage-1 -> Stage-2 compiled-model chaininget hajtson vegre, aszimmetrikus KV ownershippel es 128k context ceilinggel.

**Independent Test**: Egy split profilos, nem placeholder keres a generated split artifacttal teljesen vegigfut compiled ketstage-es uton, a health `request_path_mode=compiled` allapotot mutat, es a `validated_context_tokens` eleri a `131072` erteket.

### Tests for User Story 2

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T075 [P] [US2] Add compiled split request-path integration coverage in tests/integration/test_manual_split_compiled_request_path.cpp
- [x] T076 [P] [US2] Add stage tensor handoff contract unit coverage in tests/unit/test_split_tensor_handoff.cpp
- [x] T077 [P] [US2] Add 128k split budget integration coverage in tests/integration/test_manual_split_128k_budget.cpp
- [x] T078 [P] [US2] Add compiled split 128k smoke validation in tests/smoke/test_manual_split_compiled_128k.ps1
- [x] T079 [P] [US2] Add compiled split handoff benchmark scenario in benchmarks/steady-state/benchmark_manual_split_compiled_handoff.cpp

### Implementation for User Story 2

- [x] T080 [P] [US2] Create compiled split infer-chain helpers in include/masodik/split/split_infer_chain.hpp and src/split/split_infer_chain.cpp
- [x] T081 [US2] Replace the manual split placeholder response path with compiled Stage-1 -> Stage-2 tensor chaining in src/genai/llm_pipeline_service.cpp
- [x] T082 [US2] Implement explicit RAM-backed hidden-state transfer and handoff buffer lifecycle in src/split/split_infer_chain.cpp and src/genai/llm_pipeline_service.cpp
- [x] T083 [US2] Implement asymmetric Stage-1/Stage-2 KV budget enforcement for the 18/26 split in include/masodik/genai/generation_config.hpp, src/genai/llm_pipeline_service.cpp, and config/profiles/b580-manual-split.json
- [x] T084 [US2] Surface `request_path_mode`, `stage_1_device`, `stage_2_device`, `split_artifact_version`, and `validated_context_tokens` in include/masodik/api/openai_contract.hpp, src/api/openai_contract.cpp, and src/runtime/readiness_reporter.cpp
- [x] T085 [US2] Add explicit GPU.1/GPU.0 assignment diagnostics and loud failure on compiled split mismatch in src/genai/llm_pipeline_service.cpp and src/diagnostics/runtime_error_reporter.cpp
- [x] T086 [US2] Document compiled handoff semantics and 128k operating envelope in docs/troubleshooting.md and specs/002-manual-model-split/quickstart.md
- [x] T097 [US2] Add tokenizer-backed prompt encode and detokenized compiled split output in src/genai/llm_pipeline_service.cpp
- [x] T098 [US2] Tighten real-artifact compiled smoke validation so `Compiled split infer sikeres...` no longer counts as acceptable output in tests/smoke/test_manual_split_compiled_128k.ps1 and tools/scripts/run-smoke-suite.ps1
- [x] T099 [US2] Document the detokenized-output expectation and tokenizer artifact dependency in docs/troubleshooting.md and specs/002-manual-model-split/quickstart.md

**Checkpoint**: A manual split request-ut mar compiled ketstage-es servinget ad placeholder nelkul, es a 128k ceiling szerzodesszinten is lathato.

---

## Phase 9: User Story 3 - Ket-GPU-s proof es lezaro diagnosztika (Priority: P3)

**Goal**: A rendszer operatori szinten is bizonyitsa a B580 + iGPU egyideju reszvetelet, a compiled request-utot, es a 128k validaciohoz tartozo hardveres allapotot.

**Independent Test**: Egy proof kor alatt a health payload, a strukturalt log, es a proof export egyszerre mutatja a compiled split request-utot, a split artifact verziot, es a ket GPU megfigyelt aktivitasat.

### Tests for User Story 3

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T087 [P] [US3] Add hardware-proof health payload integration coverage in tests/integration/test_manual_split_hardware_proof.cpp
- [x] T088 [P] [US3] Add two-GPU proof smoke validation in tests/smoke/test_manual_split_hardware_proof.ps1
- [x] T089 [P] [US3] Add split stability proof benchmark scenario in benchmarks/queue-soak/benchmark_manual_split_stability_proof.cpp

### Implementation for User Story 3

- [x] T090 [P] [US3] Extend diagnostic snapshot fields for compiled proof metadata in include/masodik/runtime/health_snapshot.hpp and src/runtime/readiness_reporter.cpp
- [x] T091 [P] [US3] Emit proof-oriented runtime logs and benchmark metadata in src/diagnostics/logger.cpp, src/diagnostics/runtime_error_reporter.cpp, and tools/profiling/export-benchmark-results.ps1
- [x] T092 [US3] Implement hardware proof capture workflow for B580 VRAM and iGPU shared-memory observations in tools/scripts/capture-manual-split-proof.ps1 and tests/smoke/test_manual_split_hardware_proof.ps1
- [x] T093 [US3] Add operator-facing proof interpretation notes in docs/diagnostics-contract.md, docs/performance-baseline.md, and specs/002-manual-model-split/quickstart.md

**Checkpoint**: A split serving vegre nemcsak elmeleti, hanem operatori es benchmark-szinten is bizonyitottan ket-GPU-s uzem.

---

## Phase 10: Final Completion & Cross-Cutting Concerns

**Purpose**: A generated split artifact, a compiled request-ut, es a ket-GPU-s proof teljes workflow-janak osszefuzese a vegso release-lezarashoz

- [x] T094 [P] Add generated split artifact and compiled proof validation paths in tools/scripts/validate-quickstart.ps1 and tools/scripts/run-smoke-suite.ps1
- [x] T095 [P] Add generated split artifact and proof aggregation targets in tests/CMakeLists.txt, benchmarks/CMakeLists.txt, and tools/scripts/build-and-test.ps1
- [x] T096 Add final completion handoff notes for manual split serving in README.md, docs/local-startup.md, and docs/performance-baseline.md

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Nincs fuggese - azonnal indithato
- **Foundational (Phase 2)**: A Setup fazisra epul - BLOKKOL minden user story-t
- **User Story 1 (Phase 3)**: A Foundational fazis utan indulhat - MVP
- **User Story 2 (Phase 4)**: A Foundational fazis utan elindithato, de a legkisebb kockazatu sorrend szerint az US1 split startup alapjaira epul
- **User Story 3 (Phase 5)**: A Foundational fazis utan elindithato, de a health es failure-scope logika az US1/US2 altal bevezetett split runtime allapotokra tamaszkodik
- **Polish (Phase 6)**: Az osszes kivant user story lezarsa utan fut
- **User Story 1 closure (Phase 7)**: A Phase 6 baseline utan indul; ez allitja elo a valodi split artifactot, amelyre a kesobbi compiled request-ut epul
- **User Story 2 closure (Phase 8)**: A Phase 7 generated split artifactjara epul; placeholder nelkuli compiled chaining nelkul nem tekintheto kesznek
- **User Story 3 closure (Phase 9)**: A Phase 8 compiled split request-utjara epul; ez adja a ket-GPU-s operatori es benchmark proofot
- **Final Completion (Phase 10)**: A Phase 7-9 teljes workflow-jat zarja release-szintu validacios reteggel

### User Story Dependencies

- **User Story 1 (P1)**: Foundational utan azonnal indithato; nincs mas user story fuggese
- **User Story 2 (P2)**: Foundational utan technikailag kezdheto, de gyakorlatban az US1 stage-loader es startup-gating alapjaira epul
- **User Story 3 (P3)**: Foundational utan kezdheto, de a legertelmesebb sorrendben az US1 es US2 altal letrehozott split state-eket es handoff hibautakat diagnosztizalja
- **US1 closure**: A mar meglevo startup/manifest bazisra epul, de a feature valodi lezarasahoz kotelezo, mert ez hozza letre a generated split artifactot
- **US2 closure**: Az US1 closure utan indulhat, mert a compiled handoffhoz mar valodi stage-1/stage-2 IR-ek kellenek
- **US3 closure**: Az US2 closure utan indulhat, mert a ket-GPU-s proof csak valodi compiled request-uttal hiteles

### Within Each User Story

- Tesztek es benchmarkok elobb keszuljenek el, es kezdetben hibazzanak
- DTO-k/config szerzodesek elobb, mint a runtime control flow
- Core split runtime elobb, mint a tuning es dokumentacio
- Health, error mapping, es diagnosztika a story lezarsa elott kotelezo

### Parallel Opportunities

- **Setup**: T002-T006 jol parhuzamosithato
- **Foundational**: T008-T018 kozul tobb feladat parhuzamosithato, ha a serving profile es health DTO konfliktusokat kerulik
- **US1**: T019-T024 tesztek nagyresze parhuzamosithato; T025-T027 szinten parhuzamosan haladhat
- **US2**: T033-T038 tesztek parhuzamosithatoak; T039-T041 kulon fajlteruleteken haladhat
- **US3**: T046-T050 tesztek parhuzamosithatoak; T051-T054 kulon health/log/error retegben parhuzamosithato
- **Polish**: T057-T062 nagyreszt parhuzamos dokumentacios es futtatoi feladat
- **US1 closure**: T063-T068 jol parhuzamosithato a split builder, manifest writer, smoke es benchmark retegek kozott
- **US2 closure**: T075-T080 es T084-T085 kulon integration, benchmark, API, es diagnostics retegeken haladhat
- **US3 closure**: T087-T091 parhuzamosithato a proof smoke, benchmark export, es snapshot/log bovitesei kozott
- **Final Completion**: T094-T095 parhuzamosan vegezheto, T096 a legvegere marad

---

## Parallel Example: User Story 1

```text
Task: "Add split manifest parsing and validation unit coverage in tests/unit/test_model_repository_split_manifest.cpp"
Task: "Add split serving profile loading coverage in tests/unit/test_serving_profile_split.cpp"
Task: "Add split cold-start benchmark in benchmarks/cold-start/benchmark_manual_split_cold_start.cpp"
Task: "Implement split startup smoke validation in tests/smoke/test_manual_split_startup.ps1"
```

## Parallel Example: User Story 2

```text
Task: "Add handoff lifecycle unit coverage in tests/unit/test_manual_split_handoff.cpp"
Task: "Add split long-context integration coverage in tests/integration/test_manual_split_long_context.cpp"
Task: "Add split handoff throughput benchmark in benchmarks/steady-state/benchmark_manual_split_handoff_latency.cpp"
Task: "Implement split long-context smoke validation in tests/smoke/test_manual_split_long_context.ps1"
```

## Parallel Example: User Story 3

```text
Task: "Add split health payload serialization unit coverage in tests/unit/test_manual_split_health_payload.cpp"
Task: "Add split runtime failure scope integration coverage in tests/integration/test_manual_split_failure_scope.cpp"
Task: "Add split diagnostics benchmark correlation scenario in benchmarks/steady-state/benchmark_manual_split_diagnostics_overhead.cpp"
Task: "Implement stage diagnostic snapshot assembly in include/masodik/runtime/health_snapshot.hpp, src/runtime/readiness_reporter.cpp, and src/genai/llm_pipeline_service.cpp"
```

## Parallel Example: User Story 1 Closure

```text
Task: "Add split artifact builder unit coverage in tests/unit/test_split_artifact_builder.cpp"
Task: "Add offline split artifact smoke validation in tests/smoke/test_generate_manual_split_artifact.ps1"
Task: "Create split artifact builder types in include/masodik/split/split_artifact_builder.hpp and src/split/split_artifact_builder.cpp"
Task: "Create split artifact export DTOs in include/masodik/split/split_artifact_manifest_writer.hpp and src/split/split_artifact_manifest_writer.cpp"
```

## Parallel Example: User Story 2 Closure

```text
Task: "Add compiled split request-path integration coverage in tests/integration/test_manual_split_compiled_request_path.cpp"
Task: "Add compiled split 128k smoke validation in tests/smoke/test_manual_split_compiled_128k.ps1"
Task: "Create compiled split infer-chain helpers in include/masodik/split/split_infer_chain.hpp and src/split/split_infer_chain.cpp"
Task: "Surface request_path_mode and validated_context_tokens in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp"
```

## Parallel Example: User Story 3 Closure

```text
Task: "Add hardware-proof health payload integration coverage in tests/integration/test_manual_split_hardware_proof.cpp"
Task: "Add split stability proof benchmark scenario in benchmarks/queue-soak/benchmark_manual_split_stability_proof.cpp"
Task: "Extend diagnostic snapshot fields for compiled proof metadata in include/masodik/runtime/health_snapshot.hpp and src/runtime/readiness_reporter.cpp"
Task: "Emit proof-oriented runtime logs and benchmark metadata in src/diagnostics/logger.cpp and tools/profiling/export-benchmark-results.ps1"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1
4. Validate split startup readiness, strict fallback tiltast, es cold-start baseline-t
5. Rogzitsd a baseline mereseket, mielott a long-context vagy diagnosztikai bovitesek kovetkeznek

### Incremental Delivery

1. **US1**: ket-stage startup, split artifact validacio, strict ready semantics
2. **US2**: explicit handoff, bounded memory, long-context stability
3. **US3**: stage-szintu health, failure scope, operatori diagnosztika
4. **Polish**: baseline export, smoke-suite integracio, teljes dokumentacios zaras
5. **US1 closure**: generated split artifact workflow, checksum, konkret 18/26 reteghatar
6. **US2 closure**: placeholder kivezetese, compiled stage chaining, 128k ceiling
7. **US3 closure**: ket-GPU-s proof, stability proof bundle, operatori diagnosis
8. **Final completion**: egyetlen vegigfuto quickstart/build/proof release workflow

### Suggested MVP Scope

Az aktualis ajanlott MVP scope mar nem a korabbi Phase 3, hanem a **User Story 1 closure (Phase 7)**. A feature valodi ertekehez minimum egy generated split artifact kell; enelkul az US2 es US3 korabbi eredmenyei csak reszben tekinthetok lezartnak.

---

## Phase 11: User Story 4 — 128k Deep Split Profil (Priority: P1)

**Goal**: Uj profil letrehozasa 128k kontextus ablakkal, 18 reteg a dGPU-n (0-17) es 6 reteg + tail az iGPU-n (18-23+tail). A meglevo 32k profil valtoztalan marad.

**Independent Test**: A 128k deep split profillal inditott szolgaltatas ready allapotba kerul, egy ~100k token hosszu keres sikeresen vegigfut, a peak RAM hasznalat stabil, es a token generalasi sebesseg legalabb 1 tok/s.

### Setup: 128k Deep Split Profil

- [x] T100 [P] [US4] Uj profil letrehozasa a config/profiles/b580-deep-split-128k.json fajlban (split hatar: layer 18, desired_context_tokens: 131072, stage_1_memory_budget_gb: 12.0, stage_2_memory_budget_gb: 8.0, kv_cache_precision: u8)
- [x] T101 [P] [US4] Unit teszt: 128k profil betoltes validacio a tests/unit/test_serving_profile_128k.cpp fajlban

### Artifact generalas

- [x] T102 [US4] Split artifact generalas a 18-reteg hatarrral a tools/scripts/generate-manual-split-artifact.ps1 scripttel az uj profilhoz — kimenet: build/generated-manual-split-cut18-128k-u8/
- [x] T103 [US4] Split manifest validacio: stage-1=0-17, stage-2=18-23, validated_context_tokens=131072, checksum=fnv1a64:822f4ad58e92ee59

### Szerver validacio

- [x] T104 [US4] Szerver inditas a 128k profillal es health ellenorzes — mindket stage ready, embedding NPU is ready, port 29012
- [x] T105 [US4] Rovid keres teszt (par mondat) a 128k profilon — valasz erkezik ("Toban van ket balator")
- [ ] T106 [US4] Hosszu keres teszt (~100k token prompt) — valasz erkezik, gep stabil marad, peak RAM < fizikai memoria 80% — HALASZTVA (human validacio soran)

### Benchmark es diagnosztika

- [ ] T107 [P] [US4] Benchmark: tok/s meres 128k context-nel a benchmarks/steady-state/benchmark_128k_deep_split.cpp fajlban — HALASZTVA
- [x] T108 [P] [US4] Smoke teszt: 128k deep split validacio a tests/smoke/test_128k_deep_split.ps1 fajlban — 3/3 PASS
- [ ] T109 [US4] Memoria monitoring: peak RAM, dGPU VRAM, iGPU shared memory naplozasa — HALASZTVA

### Finomhangolas

- [ ] T110 [US4] Ha a 18 reteg nem optimalis: alternativ reteghatarak probalkozas (16 vagy 19), profil es artifact ujrageneralas
- [ ] T111 [US4] Magyar racionale: 128k deep split dontesek es memoria-szamitasok dokumentalasa a src/genai/llm_pipeline_service.cpp es docs/performance-baseline.md fajlokban

**Checkpoint**: A 128k deep split profil mukodik, a modell 1-2 tok/s sebessegel general, es a memoria stabil.

---

## Phase 12: 128k Polish & Lezaras

**Purpose**: A 128k deep split veglegesitese, regresszio ellenorzes, dokumentacio

- [ ] T112 [P] A 32k profil regresszio ellenorzese — a meglevo tesztek mind PASS maradnak
- [ ] T113 [P] Specs artifaktok frissitese: plan.md, quickstart.md, contracts/ vegso allapot a specs/002-manual-model-split/ konyvtarban
- [ ] T114 [P] CLAUDE.md es AGENTS.md frissitese a 128k profil informacioval
- [ ] T115 Smoke suite bovitese: 128k deep split integralasa a tools/scripts/run-smoke-suite.ps1 fajlba

---

## Dependencies & Execution Order (US4 bovites)

### Phase 11 Dependencies

- **Phase 11 (US4)**: A Phase 10 lezarasara es a meglevo 32k infrastrukturara epul
- **T102 (artifact generalas)**: T100 (profil) utan indulhat
- **T104-T106 (szerver validacio)**: T102 (artifact) utan indulhat
- **T107-T109 (benchmark)**: T104 (szerver) utan indulhat
- **T110-T111 (finomhangolas)**: T106-T109 eredmenyei alapjan

### Phase 12 Dependencies

- **Phase 12**: A Phase 11 osszes validacios feladatanak teljesulese utan

---

## Notes

- [P] task = mas fajlokat erint, nincs kozvetlen blokkolt fuggese
- Minden user story kulon is validalhato, de az ajanlott szallitas US1 -> US2 -> US3 sorrend
- A split feature kulcsa az explicit topology, nem a HETERO-ra bizott automatikus felosztas
- Magyar komment es rovid racionalek kotelezoek minden nem trivialis stage, memory, es diagnostics dontesnel
- A benchmark eredmenyeket mindig a celgepen, ugyanazzal a profillal kell rogzitani
- A feature vegso lezarasahoz a `request_path_mode=compiled` es a ket-GPU-s proof kotelezo; placeholder vagy monolitikus PASS nem elfogadhato
