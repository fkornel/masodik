---

description: "Task list for Helyi hibrid AI kiszolgalas"
---

# Tasks: Helyi hibrid AI kiszolgalas

**Input**: Design documents from `/specs/001-openvino-gpu-serving/`  
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/, quickstart.md

**Tests**: A baseline serving reteg mar kapott unit/integration teszteket; a streaming, tool-calling es request audit logging scope ehhez kepest tovabbi unit, integration, smoke es benchmark lefedettseget igenyel.

**Organization**: Ez a tasklista megorzi a mar elvegzett baseline serving, streaming es tool-calling munkat kipipalt elozmenykent, es a request audit logging uj feladatait ezek utan, a lista vegere fuzve vezeti be.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Parhuzamosan vegezheto, ha mas fajlokat erint es nincs kozvetlen blokkoló fuggese
- **[Story]**: Az adott user storyhoz tartozo feladat (US1, US2, US3, US4)
- Minden feladat konkret fajlutvonalat tartalmaz

## Path Conventions

- **Native C++ project**: `include/`, `src/`, `tests/`, `benchmarks/` a repository gyokerben
- **Serving/runtime modules**: `include/masodik/api/`, `include/masodik/genai/`, `include/masodik/runtime/`, `include/masodik/server/`, valamint a megfelelo `src/` alkonyvtarak
- **Feature docs**: `specs/001-openvino-gpu-serving/`
- **Smoke and validation scripts**: `tests/smoke/` es `tools/scripts/`

## Phase 1: Setup (Completed Baseline)

**Purpose**: A repository bootstrap, a helyi build-folyamat es a baseline serving profilok letrehozasa

- [x] T001 Bootstrap the native C++ project skeleton in CMakeLists.txt and CMakePresets.json
- [x] T002 Add local configure/build automation in tools/scripts/configure-local.ps1 and tools/scripts/run-smoke-suite.ps1
- [x] T003 Materialize baseline local serving profiles and startup guidance in config/profiles/b580-cpu-hetero.json, config/profiles/b580-gpu-only.json, config/profiles/b580-igpu-parallel.json, and docs/local-startup.md

---

## Phase 2: Foundational (Completed Baseline)

**Purpose**: A kozos runtime, OpenAI contract es HTTP primitives megteremtese a baseline local servinghez

- [x] T004 Implement runtime and profile configuration loading in include/masodik/runtime/service_config.hpp, src/runtime/service_config.cpp, include/masodik/genai/generation_config.hpp, and src/runtime/runtime_discovery.cpp
- [x] T005 Implement HTTP server and route registration primitives in include/masodik/server/http_server.hpp, src/server/http_server.cpp, include/masodik/server/openai_handlers.hpp, and src/server/openai_handlers.cpp
- [x] T006 Implement OpenAI request/response contract parsing and serialization in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T007 Implement startup validation, dependency validation, and readiness reporting in include/masodik/runtime/startup_validation.hpp, src/runtime/startup_validation.cpp, include/masodik/runtime/service_dependency_validator.hpp, src/runtime/service_dependency_validator.cpp, include/masodik/runtime/readiness_reporter.hpp, and src/runtime/readiness_reporter.cpp
- [x] T008 Implement the local GenAI pipeline service and device profile evaluation in include/masodik/genai/llm_pipeline_service.hpp, src/genai/llm_pipeline_service.cpp, include/masodik/genai/device_profile_evaluator.hpp, and src/genai/device_profile_evaluator.cpp
- [x] T009 Implement queueing, timeout, and degraded-state primitives in include/masodik/runtime/request_scheduler.hpp, src/runtime/request_scheduler.cpp, include/masodik/runtime/request_timeout_policy.hpp, src/runtime/request_timeout_policy.cpp, include/masodik/runtime/degraded_state_guard.hpp, and src/runtime/degraded_state_guard.cpp

**Checkpoint**: A baseline serving reteg kozos futtatasi, HTTP es szerzodeses alapjai keszen allnak.

---

## Phase 3: User Story 1 - Helyi AI API eleres (Completed Baseline)

**Goal**: A helyi kliens kepes legyen OpenAI-kompatibilis nem-streamelt chat, health es model lekerdezeseket kuldeni a helyi szervernek

**Independent Test**: A helyi kliens a vegpont atallitasa utan sikeresen kap hasznalhato JSON valaszt a chat, health es models kereseire.

- [x] T010 [P] [US1] Add baseline HTTP route coverage in tests/integration/test_http_routes.cpp and tests/integration/test_health_endpoint.cpp
- [x] T011 [P] [US1] Add baseline chat completion route coverage in tests/integration/test_chat_completion_route.cpp
- [x] T012 [US1] Implement baseline non-streaming chat/model/health behavior in src/server/openai_handlers.cpp and src/genai/llm_pipeline_service.cpp
- [x] T013 [US1] Add OpenAI-compatible minimal request parsing and payload serialization coverage in tests/unit/test_openai_contract.cpp and src/api/openai_contract.cpp
- [x] T014 [US1] Add OpenClaw-compatible `messages[].content` text-part array support in src/api/openai_contract.cpp and tests/unit/test_openai_contract.cpp

**Checkpoint**: A helyi OpenAI-kompatibilis, nem-streamelt baseline API hasznalhato.

---

## Phase 4: User Story 2 - Uzembiztos helyi futas (Completed Baseline)

**Goal**: A szolgaltatas stabilabban induljon, a hibak lathatoak legyenek, es a diagnosztika segitse a hibakeresest

**Independent Test**: Inditasi es runtime hiba eseten a szolgaltatas nem marad csendes rossz allapotban, a hibak logolva vannak, es az allapotvisszajelzes ertelmezheto.

- [x] T015 [P] [US2] Add startup state machine coverage in tests/unit/test_startup_state_machine.cpp and src/runtime/startup_state_machine.cpp
- [x] T016 [P] [US2] Add missing-runtime and missing-model integration coverage in tests/integration/test_missing_genai_runtime.cpp, tests/integration/test_missing_model_package.cpp, and tests/unit/test_runtime_dependency_report.cpp
- [x] T017 [US2] Implement launcher logging and runtime error reporting in src/main/launcher_app.cpp, include/masodik/diagnostics/logger.hpp, src/diagnostics/logger.cpp, include/masodik/diagnostics/runtime_error_reporter.hpp, and src/diagnostics/runtime_error_reporter.cpp
- [x] T018 [US2] Harden Windows crash handling and detailed exception diagnostics in src/main/launcher_app.cpp

**Checkpoint**: A baseline startup, hibaallapot es diagnosztika uzemeltethetobb lett.

---

## Phase 5: User Story 3 - Hibrid gyorsitas stabil kiszolgalassal (Completed Baseline)

**Goal**: A B580-ra celzott HETERO/GPU profilok tenylegesen stabilan induljanak es merhetoek legyenek

**Independent Test**: A rendszer kepes GPU.1-re celzott pure GPU es HETERO profilokkal indulni, a health allapot ready, es a baseline hosszabb context tesztek lefutnak.

- [x] T019 [P] [US3] Correct the target device selection for B580 profiles in config/profiles/b580-cpu-hetero.json, config/profiles/b580-gpu-only.json, and config/profiles/b580-igpu-parallel.json
- [x] T020 [P] [US3] Map RelWithDebInfo builds to Release OpenVINO binaries in cmake/dependencies/OpenVINO.cmake and tools/scripts/configure-local.ps1
- [x] T021 [P] [US3] Align cache usage for stable HETERO startup in config/profiles/b580-cpu-hetero.json and build/cache/
- [x] T022 [P] [US3] Validate GPU-only and HETERO startup behavior in tests/integration/test_hybrid_profile_startup.cpp and tests/integration/test_long_context_32768.cpp
- [x] T023 [US3] Pin the active HETERO serving endpoint to port 29011 in config/profiles/b580-cpu-hetero.json

**Checkpoint**: A baseline HETERO local serving a B580 gepen mukodokepes es a rogzitett porton elerheto.

---

## Phase 6: Setup (Streaming Delta)

**Purpose**: A streaming fejleszteshez szukseges kozos fixture-ok, smoke helper-ek es futtatoi bekotesek elokeszitese

- [x] T024 Create streaming request fixtures in tests/fixtures/chat_request_stream.json and tests/fixtures/chat_request_stream_abort.json
- [x] T025 [P] Create reusable SSE smoke client helper in tests/smoke/support/stream_client.ps1
- [x] T026 [P] Register streaming smoke and benchmark entry points in tests/CMakeLists.txt, benchmarks/CMakeLists.txt, and tools/scripts/run-smoke-suite.ps1

---

## Phase 7: Foundational (Streaming Delta)

**Purpose**: Azok a kozos streaming primitivek es szerzodesek, amelyeknek keszen kell lenniuk barmely streaming story implementalasa elott

**⚠️ CRITICAL**: A streaming user story fazisok csak ezen alapok utan indulhatnak

- [x] T027 Define OpenAI-compatible SSE success, `[DONE]`, and abort semantics in specs/001-openvino-gpu-serving/contracts/service-contract.md
- [x] T028 [P] Define streaming DTO-k es SSE serializer felulet in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T029 [P] Extend HTTP response primitives for `text/event-stream`, chunked writes, and disconnect detection in include/masodik/server/http_server.hpp and src/server/http_server.cpp
- [x] T030 [P] Add stream callback and cancellation primitives in include/masodik/genai/llm_pipeline_service.hpp and include/masodik/runtime/request_scheduler.hpp
- [x] T031 [P] Add stream-aware queue and health diagnostics primitives in include/masodik/diagnostics/queue_metrics.hpp, src/diagnostics/queue_metrics.cpp, include/masodik/runtime/health_snapshot.hpp, and src/runtime/readiness_reporter.cpp
- [x] T032 Define pre-stream JSON errors vs in-stream terminal SSE errors in include/masodik/server/openai_error_mapper.hpp, src/server/openai_error_mapper.cpp, and docs/diagnostics-contract.md

**Checkpoint**: A streaming szerzodes, a HTTP streaming alapok es a kozos runtime primitivek keszen allnak.

---

## Phase 8: User Story 1 - Helyi AI API eleres (Streaming MVP) 🎯 MVP

**Goal**: A `/v1/chat/completions` vegpont OpenAI-kompatibilis SSE streamet adjon `stream=true` eseten, mikozben a `stream=false` JSON valasz tovabbra is hasznalhato marad

**Independent Test**: Egy helyi kliens ugyanarra a vegpontra `stream=true` kuldessel `text/event-stream` valaszt, tobb `data:` esemenyt es szabalyos `data: [DONE]` lezarat kap; ugyanaz a keres `stream=false` mellett tovabbra is egyetlen JSON valaszkent ter vissza.

### Tests for User Story 1

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T033 [P] [US1] Add SSE serialization coverage for `chat.completion.chunk` payloads and `[DONE]` sentinel in tests/unit/test_openai_contract.cpp
- [x] T034 [P] [US1] Add integration coverage for `stream=true` headers, event ordering, and `stream=false` parity in tests/integration/test_chat_completion_route.cpp
- [x] T035 [P] [US1] Create end-to-end SSE smoke validation in tests/smoke/test_chat_completion_stream.ps1
- [x] T036 [P] [US1] Add first-token latency benchmark for `stream=true` in benchmarks/steady-state/benchmark_stream_first_token.cpp

### Implementation for User Story 1

- [x] T037 [P] [US1] Implement `chat.completion.chunk` builders, delta payload serialization, and terminal `[DONE]` emission in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T038 [P] [US1] Implement token-by-token streaming callback support in include/masodik/genai/llm_pipeline_service.hpp and src/genai/llm_pipeline_service.cpp
- [x] T039 [US1] Implement `stream=true` route branching and SSE response emission in src/server/openai_handlers.cpp and src/server/http_server.cpp
- [x] T040 [US1] Keep `stream=false` completion semantics aligned with the streaming path in src/server/openai_handlers.cpp and src/api/openai_contract.cpp
- [x] T041 [US1] Update streaming quickstart example and fixture usage in specs/001-openvino-gpu-serving/quickstart.md and tests/fixtures/chat_request_stream.json

**Checkpoint**: A helyi OpenAI-kompatibilis szerver mar szabalyos SSE chat streamet ad, es a nem streamelt chat viselkedes sem torik el.

---

## Phase 9: User Story 2 - Uzembiztos helyi futas (Streaming Robustness)

**Goal**: A stream inditasi hiba, futas kozbeni hiba es kliens abort eseten a rendszer egyertelmuen jelezzen, es ne hagyjon csendes elakadast vagy bent ragadt futast maga utan

**Independent Test**: Ha a szolgaltatas nem `ready`, hibas modellazonosito erkezik, a generalas kozben hiba lep fel, vagy a kliens megszakitja az olvasast, akkor a rendszer vagy meg a stream elott JSON hibavalaszt ad, vagy a streamet explicit terminalis hibajelzessel zarja le, es a request lease felszabadul.

### Tests for User Story 2

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T042 [P] [US2] Add unit coverage for stream-start and mid-stream error payload mapping in tests/unit/test_openai_stream_errors.cpp
- [x] T043 [P] [US2] Add integration tests for not-ready, model-mismatch, timeout, and generator-failure streaming cases in tests/integration/test_streaming_failure_modes.cpp
- [x] T044 [P] [US2] Create client-abort smoke validation in tests/smoke/test_stream_abort.ps1

### Implementation for User Story 2

- [x] T045 [P] [US2] Track active streams, cancellation, and disconnect cleanup in include/masodik/runtime/request_scheduler.hpp, src/runtime/request_scheduler.cpp, include/masodik/diagnostics/queue_metrics.hpp, and src/diagnostics/queue_metrics.cpp
- [x] T046 [P] [US2] Emit pre-stream JSON errors and in-stream terminal SSE error events from include/masodik/server/openai_error_mapper.hpp, src/server/openai_error_mapper.cpp, include/masodik/api/openai_contract.hpp, and src/api/openai_contract.cpp
- [x] T047 [US2] Detect socket write failures and abort generation without silent hangs in src/server/http_server.cpp, src/server/openai_handlers.cpp, and src/genai/llm_pipeline_service.cpp
- [x] T048 [US2] Publish stream abort and failure troubleshooting guidance in docs/troubleshooting.md and specs/001-openvino-gpu-serving/contracts/service-contract.md

**Checkpoint**: A streaming hibak, abortok es nem-ready helyzetek kliensoldalrol jol lathatoak, a szerveroldali eroforrasok pedig rendezettek maradnak.

---

## Phase 10: User Story 3 - Hibrid gyorsitas stabil kiszolgalassal (Mixed Load)

**Goal**: A streamelt es nem streamelt kerek vegyes terheles alatt is kovetkezetesen mukodjenek, a health/queue allapot lathato maradjon, es a hosszabb eletu streamek ne torjek meg a hybrid serving stabilitasat

**Independent Test**: Vegyes `stream=true` es `stream=false` keresor alatt a rendszer megorzi a `ready/degraded` allapot szemantikajat, a queue/stream diagnosztika lathato, es a streamek nem eheztetik ki a hagyomanyos valaszokat csendes degradacioval.

### Tests for User Story 3

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T049 [P] [US3] Add mixed-mode integration coverage for concurrent streaming and non-streaming chat requests in tests/integration/test_mixed_streaming_requests.cpp
- [x] T050 [P] [US3] Add streaming queue visibility smoke validation in tests/smoke/test_stream_queue_visibility.ps1
- [x] T051 [P] [US3] Add mixed streaming/non-streaming soak benchmark in benchmarks/queue-soak/benchmark_streaming_mixed_load.cpp

### Implementation for User Story 3

- [x] T052 [P] [US3] Extend health and diagnostics payloads with active-stream counters and last-stream-failure fields in include/masodik/api/openai_contract.hpp, src/api/openai_contract.cpp, include/masodik/runtime/health_snapshot.hpp, and src/runtime/readiness_reporter.cpp
- [x] T053 [P] [US3] Tune timeout budgeting and scheduler fairness for long-lived streams in include/masodik/runtime/request_timeout_policy.hpp, src/runtime/request_timeout_policy.cpp, include/masodik/runtime/request_scheduler.hpp, and src/runtime/request_scheduler.cpp
- [x] T054 [US3] Preserve `ready/degraded` state semantics under mixed streaming load in src/server/openai_handlers.cpp, src/runtime/service_lifecycle.cpp, and src/runtime/degraded_state_guard.cpp
- [x] T055 [US3] Update mixed-mode validation notes in config/profiles/b580-cpu-hetero.json, config/profiles/b580-gpu-only.json, and docs/performance-baseline.md

**Checkpoint**: A streaming tamogatas a jelenlegi hybrid/local serving celokkal egyutt is stabilan es merhetoen uzemel.

---

## Phase 11: Polish & Cross-Cutting Concerns

**Purpose**: A streaming tamogatas teljes feature-szintu befejezese, regresszio-vedelme es uzemeltetesi dokumentalasa

- [x] T056 [P] Add streaming regression steps to tools/scripts/validate-quickstart.ps1 and tools/scripts/run-smoke-suite.ps1
- [x] T057 [P] Add streaming usage notes to docs/local-startup.md and README.md
- [x] T058 Add final fixture and smoke cleanup for stream/non-stream parity in tests/fixtures/chat_request.json, tests/smoke/test_chat_completion.ps1, and tests/smoke/test_chat_completion_stream.ps1

---

## Phase 12: Setup (Tool Calling Delta)

**Purpose**: A tool calling fejleszteshez szukseges kozos fixture-ok, smoke helper-ek es futtatoi bekotesek elokeszitese

- [x] T059 Create tool-calling request fixtures in tests/fixtures/chat_request_tool_call.json, tests/fixtures/chat_request_tool_call_stream.json, and tests/fixtures/chat_request_tool_choice_required.json
- [x] T060 [P] Create reusable tool-call smoke assertion helpers in tests/smoke/support/tool_call_assertions.ps1 and tests/smoke/support/stream_client.ps1
- [x] T061 [P] Register tool-calling smoke and benchmark entry points in tests/CMakeLists.txt, benchmarks/CMakeLists.txt, tools/scripts/run-smoke-suite.ps1, and tools/scripts/validate-quickstart.ps1

---

## Phase 13: Foundational (Tool Calling Delta)

**Purpose**: Azok a kozos tool-calling primitivek es szerzodesek, amelyeknek keszen kell lenniuk a teljes tool-calling story implementalasa elott

**⚠️ CRITICAL**: A tool-calling user story fazis csak ezen alapok utan indulhat

- [x] T062 Define finalized tool-calling contract semantics for non-stream and streaming deltas in specs/001-openvino-gpu-serving/contracts/service-contract.md and docs/diagnostics-contract.md
- [x] T063 [P] Extend OpenAI DTO-k with `tools`, `tool_choice`, `tool_calls`, and tool-call delta types in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T064 [P] Add tool definition validation and request parsing primitives in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T065 [P] Add tool-call-aware generation result abstractions in include/masodik/genai/llm_pipeline_service.hpp and src/genai/llm_pipeline_service.cpp
- [x] T066 [P] Extend SSE serialization primitives for indexed tool-call deltas and terminal `finish_reason=tool_calls` handling in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T067 [P] Add tool-call-aware diagnostics and error mapping primitives in include/masodik/server/openai_error_mapper.hpp, src/server/openai_error_mapper.cpp, include/masodik/diagnostics/queue_metrics.hpp, and src/diagnostics/queue_metrics.cpp

**Checkpoint**: A tool-calling szerzodes, DTO-k, validacio es streaming primitivek keszen allnak.

---

## Phase 14: User Story 4 - Tool calling OpenAI-kompatibilis szerzodessel (Priority: P1)

**Goal**: A `/v1/chat/completions` vegpont OpenAI-kompatibilis modon fogadja a `tools`/`tool_choice` mezoket, es a modell altal valasztott tool hivast non-stream es stream uzemmodban is strukturaltan adja vissza

**Independent Test**: Egy tool-definiciot is tartalmazo keresnel a kliens non-stream uzemben ervenyes `tool_calls` tombot, stream uzemben pedig osszefuzheto tool-call SSE delta-kat kap, mikozben a szerver nem hajt vegre toolt es hibas tool payload eseten ertelmezheto hibavalaszt ad.

### Tests for User Story 4

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T068 [P] [US4] Add unit coverage for tool request parsing, schema validation, and non-stream tool-call serialization in tests/unit/test_openai_tool_call_contract.cpp
- [x] T069 [P] [US4] Add unit coverage for streaming tool-call delta serialization and terminal finish reasons in tests/unit/test_openai_tool_call_streaming.cpp
- [x] T070 [P] [US4] Add integration coverage for non-stream tool calling, invalid tool payloads, and no-execution semantics in tests/integration/test_tool_calling_route.cpp
- [x] T071 [P] [US4] Add integration coverage for streaming tool-call delta ordering and argument assembly in tests/integration/test_tool_calling_streaming.cpp
- [x] T072 [P] [US4] Create OpenClaw-style end-to-end tool-calling smoke validation in tests/smoke/test_tool_calling.ps1 and tests/smoke/test_tool_calling_stream.ps1
- [x] T073 [P] [US4] Add first-tool-call-delta benchmark in benchmarks/steady-state/benchmark_tool_call_first_delta.cpp

### Implementation for User Story 4

- [x] T074 [P] [US4] Implement tool-aware request parsing and response serialization in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T075 [P] [US4] Implement tool-call-capable generation result mapping in include/masodik/genai/llm_pipeline_service.hpp and src/genai/llm_pipeline_service.cpp
- [x] T076 [US4] Implement non-stream tool-call emission, finish-reason handling, and no-execution route behavior in src/server/openai_handlers.cpp and src/genai/llm_pipeline_service.cpp
- [x] T077 [US4] Implement streaming tool-call delta emission, indexed argument assembly, and terminal stream closure in src/server/openai_handlers.cpp, src/api/openai_contract.cpp, and src/server/http_server.cpp
- [x] T078 [US4] Surface tool-calling validation and streaming failure diagnostics in src/server/openai_error_mapper.cpp, docs/troubleshooting.md, and docs/diagnostics-contract.md

**Checkpoint**: A helyi OpenAI-kompatibilis szerver a tool callingot non-stream es stream uzemmodban is strukturaltan tovabbitja, es a kliensoldali orchestrator megbizhatoan fel tudja dolgozni.

---

## Phase 15: Polish & Cross-Cutting Concerns

**Purpose**: A tool-calling tamogatas regressziovedelme, benchmarkolasa es uzemeltetesi dokumentalasa

- [x] T079 [P] Add tool-calling quickstart, local startup, and README usage notes in specs/001-openvino-gpu-serving/quickstart.md, docs/local-startup.md, and README.md
- [x] T080 [P] Add tool-calling regression steps to tools/scripts/run-smoke-suite.ps1, tools/scripts/validate-quickstart.ps1, and tests/fixtures/chat_request.json
- [x] T081 [P] Add tool-calling performance and compatibility baseline notes in docs/performance-baseline.md and specs/001-openvino-gpu-serving/contracts/service-contract.md
- [x] T082 Add final stream/non-stream tool-calling parity cleanup in tests/smoke/test_chat_completion.ps1, tests/smoke/test_chat_completion_stream.ps1, tests/smoke/test_tool_calling.ps1, and tests/smoke/test_tool_calling_stream.ps1

---

## Phase 16: Setup (Request Audit Logging Delta)

**Purpose**: A request audit logginghoz szukseges fixture-ok, smoke helper-ek es futtatasi bekotesek elokeszitese

- [x] T083 Create request-audit fixtures for content-only, auto, required, and invalid named-tool scenarios in tests/fixtures/chat_request_audit_content_only.json, tests/fixtures/chat_request_audit_tool_auto.json, tests/fixtures/chat_request_audit_tool_required.json, and tests/fixtures/chat_request_audit_invalid_named_tool_choice.json
- [x] T084 [P] Create reusable request-audit log assertion helpers in tests/smoke/support/request_audit_assertions.ps1
- [x] T085 [P] Register request-audit smoke entry points in tools/scripts/run-smoke-suite.ps1 and tools/scripts/validate-quickstart.ps1

---

## Phase 17: Foundational (Request Audit Logging Delta)

**Purpose**: Azok a kozos audit schema- es diagnosztikai primitivek, amelyeknek keszen kell lenniuk a request audit logging story elott

**⚠️ CRITICAL**: A request audit logging user story fazis csak ezen alapok utan indulhat

- [x] T086 Define request-audit schema, decision-path values, and redaction rules in specs/001-openvino-gpu-serving/contracts/service-contract.md, specs/001-openvino-gpu-serving/data-model.md, and docs/diagnostics-contract.md
- [x] T087 [P] Add RequestAuditRecord, validation-outcome, and tool-choice summary DTO primitives in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T088 [P] Add structured request-audit logging primitives and sink formatting in include/masodik/diagnostics/logger.hpp and src/diagnostics/logger.cpp
- [x] T089 [P] Add route-support request-audit correlation plumbing in include/masodik/server/openai_handlers.hpp and src/server/openai_handlers.cpp
- [x] T090 [P] Add decision-path and validation-outcome classification helpers in include/masodik/genai/llm_pipeline_service.hpp, src/genai/llm_pipeline_service.cpp, include/masodik/server/openai_error_mapper.hpp, and src/server/openai_error_mapper.cpp

**Checkpoint**: A request audit schema, a logolasi primitivek es a route-szintu korrelacio keszen allnak.

---

## Phase 18: User Story 4 - Tool calling audit observability (Priority: P1)

**Goal**: Az uzemelteto request audit logging alapjan utolag is egyertelmuen lassa, hogy a kliens kuldott-e `tools` mezot, milyen `tool_choice` erkezett, milyen `validation_outcome` szuletett, es a szerver melyik `decision_path` agra sorolta a kerest

**Independent Test**: OpenClaw-szeru content-only, toolos, streamelt es hibas named-tool keresorok utan a naploban ugyanahhoz a `request_id`-hoz lathato a `tools_present`, `tool_count`, `tool_choice_kind`, `tool_choice_name`, `decision_path` es `validation_outcome` mezo, promptdump nelkul.

### Tests for User Story 4

> **NOTE: Ezek a tesztek keszuljenek el elobb, es kezdetben hibazzanak**

- [x] T091 [P] [US4] Add unit coverage for RequestAuditRecord derivation from content-only, auto, required, named, and invalid tool-choice payloads in tests/unit/test_openai_request_audit.cpp
- [x] T092 [P] [US4] Add integration coverage for request-audit logging on non-stream, streaming, and validation-failure chat routes in tests/integration/test_request_audit_logging.cpp
- [x] T093 [P] [US4] Create OpenClaw-style smoke validation for audit fields and request-id correlation in tests/smoke/test_request_audit_logging.ps1 and tests/smoke/support/request_audit_assertions.ps1

### Implementation for User Story 4

- [x] T094 [P] [US4] Implement RequestAuditRecord derivation and truncated message preview rules in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp
- [x] T095 [P] [US4] Implement request-id-correlated audit emission for success and validation-failure exits in src/server/openai_handlers.cpp, include/masodik/diagnostics/logger.hpp, and src/diagnostics/logger.cpp
- [x] T096 [US4] Implement explicit `decision_path` classification for `content_only`, `tool_call_auto`, `tool_call_required`, and `tool_call_named` flows in include/masodik/genai/llm_pipeline_service.hpp, src/genai/llm_pipeline_service.cpp, and src/server/openai_handlers.cpp
- [x] T097 [US4] Implement `validation_outcome` mapping for parser, tool schema, model-selection, and runtime-readiness failures in include/masodik/server/openai_error_mapper.hpp, src/server/openai_error_mapper.cpp, and src/server/openai_handlers.cpp

**Checkpoint**: Az OpenClaw-kompatibilis request audit logging megbizhatoan megmutatja a tool-calling bemenetet, a szerver dontesi agat es a validacios kimenetet.

---

## Phase 19: Polish & Cross-Cutting Concerns

**Purpose**: A request audit logging regressziovedelme, uzemeltetesi olvashatosaga es overhead-ellenorzese

- [x] T098 [P] Add request-audit quickstart and OpenClaw diagnostics guidance in specs/001-openvino-gpu-serving/quickstart.md, docs/local-startup.md, and README.md
- [x] T099 [P] Add request-audit troubleshooting and sample log interpretation notes in docs/troubleshooting.md and docs/diagnostics-contract.md
- [x] T100 [P] Add request-audit overhead and log-size baseline notes in docs/performance-baseline.md and benchmarks/steady-state/benchmark_parallel_requests.cpp
- [x] T101 Add final request-audit parity cleanup across tests/smoke/test_tool_calling.ps1, tests/smoke/test_tool_calling_stream.ps1, tests/smoke/test_request_audit_logging.ps1, and tests/integration/test_request_audit_logging.cpp

---

## Dependencies & Execution Order

### Historical Baseline

- **Phase 1-5**: Mar elvegzett alap serving munka; ezek adjak a jelenlegi kiindulasi allapotot

### Streaming Phase Dependencies

- **Phase 6 (Streaming Setup)**: Azonnal indithato a kesz baseline-re
- **Phase 7 (Streaming Foundational)**: A Phase 6 utan kovetkezik, es minden streaming storyt blokkol
- **Phase 8 (US1 streaming MVP)**: A streaming MVP; a Foundational fazis utan azonnal indithato
- **Phase 9 (US2 streaming robustness)**: A streaming hibakezeles es abort-biztonsag; US1 szerveroldali alapjaira epul
- **Phase 10 (US3 mixed load)**: Vegyes terheles, scheduler fairness es diagnosztika; US1 + US2 utan ad teljes stabilitasi kepet
- **Phase 11 (Polish)**: Az osszes kivant streaming story utan kovetkezik

### Tool Calling Phase Dependencies

- **Phase 12 (Tool Calling Setup)**: A mar kesz streaming es smoke infrastruktura utan indithato
- **Phase 13 (Tool Calling Foundational)**: A Phase 12 utan kovetkezik, es a tool-calling storyt blokkolja
- **Phase 14 (US4 tool calling)**: A tool-calling MVP; a streaming alapok es a tool-calling foundational fazis utan indithato
- **Phase 15 (Tool Calling Polish)**: A teljes tool-calling story utan kovetkezik

### Request Audit Phase Dependencies

- **Phase 16 (Request Audit Setup)**: A kesz tool-calling smoke es fixture alapon azonnal indithato
- **Phase 17 (Request Audit Foundational)**: A Phase 16 utan kovetkezik, es a request audit storyt blokkolja
- **Phase 18 (US4 request audit observability)**: A Phase 14 kesz tool-calling slice-ara es a Phase 17 audit primitivekre epul
- **Phase 19 (Request Audit Polish)**: A teljes request audit story utan kovetkezik

### User Story Dependencies

- **US1 (P1)**: Nem fugg mas streaming storytol a Foundational fazison tul; ez a javasolt streaming MVP
- **US2 (P2)**: A stream emisszios utvonal megletere epul, ezert US1 utan vagy azzal szorosan osszehangoltan erdemes szallitani
- **US3 (P3)**: A scheduler, health es benchmark tuning miatt az US1 + US2 utan validalhato ertelmesen
- **US4 (P1)**: A mar kesz streaming alapon epul; a Phase 13 tool-calling primitivek es a Phase 8-10 streaming tapasztalatai utan validalhato ertelmesen, a request audit logging delta pedig erre a kesz tool-calling slice-ra ul ra a Phase 17 utan

### Within Each User Story

- A tesztek, smoke szkriptek es benchmarkok legyenek meg elobb, es kezdetben hibazzanak
- A public DTO-k, HTTP primitivek es runtime feluletek keszuljenek el a route wire-up elott
- A generator callback es scheduler valtozasok elozzek meg a vegleges route integraciot
- A hibafelszinezes es diagnosztika keszuljon el a story lezarsa elott
- Minden story vegen fusson le legalabb az adott independent test forgatokonyv

### Parallel Opportunities

- Phase 6-ban T025 es T026 parhuzamosan vegezheto T024 utan
- Phase 7-ben T028-T032 nagy resze kulon fajlteruleteken parhuzamosan viheto
- US1-ben T033-T036 teszt/benchmark feladatok parhuzamosak, ahogy T037 es T038 is
- US2-ben T042-T044 es T045-T046 kulon szalakon is haladhat
- US3-ban T049-T051 es T052-T053 parhuzamosan futtathato, ha a csapat kapacitasa engedi
- Phase 12-ben T060 es T061 parhuzamosan vegezheto T059 utan
- Phase 13-ban T063-T067 nagy resze kulon fajlteruleteken parhuzamosan viheto
- US4-ben T068-T073 teszt/benchmark feladatok parhuzamosak, ahogy T074 es T075 is
- Phase 16-ban T084 es T085 parhuzamosan vegezheto T083 utan
- Phase 17-ben T087-T090 nagy resze kulon fajlteruleteken parhuzamosan viheto
- US4 request audit korben T091-T093 tesztfeladatok parhuzamosak, ahogy T094 es T095 is
- Phase 19-ben T098-T100 parhuzamosan viheto T097 utan

---

## Parallel Example: User Story 1

```text
Task: "Add SSE serialization coverage for `chat.completion.chunk` payloads and `[DONE]` sentinel in tests/unit/test_openai_contract.cpp"
Task: "Add integration coverage for `stream=true` headers, event ordering, and `stream=false` parity in tests/integration/test_chat_completion_route.cpp"
Task: "Implement `chat.completion.chunk` builders, delta payload serialization, and terminal `[DONE]` emission in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp"
Task: "Implement token-by-token streaming callback support in include/masodik/genai/llm_pipeline_service.hpp and src/genai/llm_pipeline_service.cpp"
```

## Parallel Example: User Story 2

```text
Task: "Add integration tests for not-ready, model-mismatch, timeout, and generator-failure streaming cases in tests/integration/test_streaming_failure_modes.cpp"
Task: "Create client-abort smoke validation in tests/smoke/test_stream_abort.ps1"
Task: "Track active streams, cancellation, and disconnect cleanup in include/masodik/runtime/request_scheduler.hpp, src/runtime/request_scheduler.cpp, include/masodik/diagnostics/queue_metrics.hpp, and src/diagnostics/queue_metrics.cpp"
Task: "Emit pre-stream JSON errors and in-stream terminal SSE error events from include/masodik/server/openai_error_mapper.hpp, src/server/openai_error_mapper.cpp, include/masodik/api/openai_contract.hpp, and src/api/openai_contract.cpp"
```

## Parallel Example: User Story 3

```text
Task: "Add mixed-mode integration coverage for concurrent streaming and non-streaming chat requests in tests/integration/test_mixed_streaming_requests.cpp"
Task: "Add mixed streaming/non-streaming soak benchmark in benchmarks/queue-soak/benchmark_streaming_mixed_load.cpp"
Task: "Extend health and diagnostics payloads with active-stream counters and last-stream-failure fields in include/masodik/api/openai_contract.hpp, src/api/openai_contract.cpp, include/masodik/runtime/health_snapshot.hpp, and src/runtime/readiness_reporter.cpp"
Task: "Tune timeout budgeting and scheduler fairness for long-lived streams in include/masodik/runtime/request_timeout_policy.hpp, src/runtime/request_timeout_policy.cpp, include/masodik/runtime/request_scheduler.hpp, and src/runtime/request_scheduler.cpp"
```

---

## Parallel Example: User Story 4

```text
Task: "Add unit coverage for tool request parsing, schema validation, and non-stream tool-call serialization in tests/unit/test_openai_tool_call_contract.cpp"
Task: "Add integration coverage for streaming tool-call delta ordering and argument assembly in tests/integration/test_tool_calling_streaming.cpp"
Task: "Implement tool-aware request parsing and response serialization in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp"
Task: "Implement tool-call-capable generation result mapping in include/masodik/genai/llm_pipeline_service.hpp and src/genai/llm_pipeline_service.cpp"
```

---

## Parallel Example: User Story 4 Audit Logging

```text
Task: "Add unit coverage for RequestAuditRecord derivation from content-only, auto, required, named, and invalid tool-choice payloads in tests/unit/test_openai_request_audit.cpp"
Task: "Add integration coverage for request-audit logging on non-stream, streaming, and validation-failure chat routes in tests/integration/test_request_audit_logging.cpp"
Task: "Implement RequestAuditRecord derivation and truncated message preview rules in include/masodik/api/openai_contract.hpp and src/api/openai_contract.cpp"
Task: "Implement request-id-correlated audit emission for success and validation-failure exits in src/server/openai_handlers.cpp, include/masodik/diagnostics/logger.hpp, and src/diagnostics/logger.cpp"
```

---

## Implementation Strategy

### Baseline Already Completed

1. Phase 1-5 mar elkeszult, ez a jelenlegi kiindulasi allapot
2. Az elozo bovitmenyek Phase 6-tal indultak; az aktualis request audit logging delta Phase 16-tal folytatodik

### MVP First (Streaming User Story 1 Only)

1. Complete Phase 6: Setup
2. Complete Phase 7: Foundational
3. Complete Phase 8: User Story 1
4. **STOP and VALIDATE**: Futtasd le a `tests/integration/test_chat_completion_route.cpp`, `tests/smoke/test_chat_completion_stream.ps1`, es `benchmarks/steady-state/benchmark_stream_first_token.cpp` forgatokonyveket
5. Csak ezutan nyiss US2/US3 munkat

### Incremental Delivery

1. Streaming Setup + Foundational -> streaming platform kesz
2. US1 -> OpenAI-kompatibilis SSE stream kepes vertical slice
3. US2 -> abort- es hibabiztos streaming
4. US3 -> mixed-load stabilitas, fairness, health es benchmark tuning
5. Tool Calling Setup + Foundational -> tool-calling platform kesz
6. US4 -> non-stream + streaming tool calling vertical slice
7. Tool Calling Polish -> dokumentalt es regresszioval vedett agentikus vegallapot
8. Request Audit Setup + Foundational -> audit schema, logolasi primitivek es korrelacio kesz
9. US4 request audit observability -> OpenClaw hibakereseshez lathato `tools` / `tool_choice` / `decision_path` / `validation_outcome`
10. Request Audit Polish -> dokumentalt, smoke-kal vedett es overhead szempontbol rogzitett audit logging

### Parallel Team Strategy

1. Egy fejleszto keszitse a HTTP/SSE primitiveket (T028-T032)
2. Egy fejleszto parhuzamosan epitse a GenAI callback es route teszteket (T033-T040)
3. A streaming MVP utan kulon szalra viheto az abort/failure (US2) es a mixed-load tuning (US3)
4. A tool-calling korben kulon szalra viheto az API/DTO reteg (T063-T066) es a GenAI/route reteg (T068-T077)
5. A request audit korben kulon szalra viheto az audit DTO/logging reteg (T087-T095) es a route/integration regressziok (T091-T093, T096-T101)

---

## Notes

- A Phase 1-5 a tenylegesen elvegzett baseline munka rekonstrualt, kipipalt nyilvantartasa
- Az uj streaming feladatok Phase 6-tal, a tool-calling feladatok Phase 12-vel, a request audit logging feladatok pedig Phase 16-tal kezdodnek, vagyis a korabbi taskok nem vesznek el
- A nem streaming JSON ut a streaming scope-ban is megorzendo szerzodes, nem regressziopont
- Az SSE implementacio OpenAI-kompatibilis `data:` eventekre, egyertelmu streamlezarasra es lathato hibafelszinezesre epuljon
- A benchmarkoknal kulon rogzitendo a first-token, first-tool-call-delta es a mixed-load viselkedes
- Kerulendo: csendes stream megszakadas, bent ragadt request lease, `stream=true` es `stream=false` kozti validacios szemantikai elteres, valamint elveszo `tool_calls` payload streaming kozben
- A request audit logging ne valjon korlatlan promptdump-pa; csak a `tools`, `tool_choice`, `decision_path`, `validation_outcome` es korlatozott preview mezok keruljenek a diagnosztikai kimenetbe
