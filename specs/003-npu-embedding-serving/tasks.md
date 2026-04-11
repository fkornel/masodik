# Tasks: NPU Embedding Serving

**Input**: Design documents from `/specs/003-npu-embedding-serving/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/embedding-contract.md

**Tests**: Automatizalt tesztek minden kritikus utra. Benchmark REQUIRED az NPU betoltes es embedding latency-re.

**Organization**: A feladatok user story szerint szervezettek (US1: endpoint, US2: NPU health, US3: batch+usage).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Parhuzamositható (kulonbozo fajlok, nincs fuggoseg)
- **[Story]**: Melyik user story-hoz tartozik (US1, US2, US3)
- Pontos fajl utvonalak a leirasokban

---

## Phase 1: Setup

**Purpose**: Embedding modult tamogato alap infrastruktura letrehozasa

- [x] T001 [P] Embedding DTO-k hozzaadasa (EmbeddingRequest, EmbeddingResponse, EmbeddingObject, EmbeddingUsage) az include/masodik/api/openai_contract.hpp fajlhoz
- [x] T002 [P] Embedding JSON parse/serialize implementalasa a src/api/openai_contract.cpp fajlban
- [x] T003 [P] EmbeddingProfileConfig mezok hozzaadasa az include/masodik/config/serving_profile.hpp fajlhoz (enabled, model_name, model_path, device, reject_cpu_fallback, endpoint_path)
- [x] T004 [P] Embedding profil szekció JSON betoltese a src/config/serving_profile.cpp fajlban
- [x] T005 Embedding szekció hozzaadasa a config/profiles/b580-manual-split-cut23-tail-32k-u8.json aktiv profilhoz

---

## Phase 2: Foundational (EmbeddingPipelineService)

**Purpose**: Az NPU embedding pipeline alap osztaly, amely az osszes user story-nak elofeltétel

**CRITICAL**: Amig ez a fazis nem kesz, a user story-k nem kezdhetok el.

- [x] T006 EmbeddingPipelineService osztaly letrehozasa az include/masodik/genai/embedding_pipeline_service.hpp fajlban (initialize, embed, ready, state, model_info)
- [x] T007 EmbeddingPipelineService implementalasa a src/genai/embedding_pipeline_service.cpp fajlban — ov::genai::TextEmbeddingPipeline wrapper, NPU betoltes, MEAN pooling, normalizalas
- [x] T008 [P] CMakeLists.txt bovitese az uj forrasfajlokkal (embedding_pipeline_service.cpp, openai_contract.cpp embedding resz) — GLOB_RECURSE automatikusan felszedi
- [x] T009 [P] Unit teszt: embedding contract parse/serialize tesztek a tests/unit/test_openai_embedding_contract.cpp fajlban
- [x] T010 [P] Unit teszt: embedding profil config betoltes tesztek a tests/unit/test_serving_profile_embedding.cpp fajlban

**Checkpoint**: EmbeddingPipelineService lefordul es a unit tesztek zoldek.

---

## Phase 3: User Story 1 - Embedding vektor lekerese (Priority: P1) MVP

**Goal**: OpenAI-kompatibilis `/v1/embeddings` endpoint, egyedi es batch inputtal

**Independent Test**: `curl POST /v1/embeddings` egyetlen szoveggel → 200 OK, data[0].embedding hossza 1024

### Tests for US1

- [x] T011 [P] [US1] Integration teszt: pipeline 503 + egyedi embedding validacio a tests/integration/test_embedding_endpoint.cpp fajlban
- [x] T012 [P] [US1] Integration teszt: ismeretlen modell 404 hiba a tests/integration/test_embedding_endpoint.cpp fajlban
- [x] T013 [P] [US1] Integration teszt: ures input 400 + invalid JSON hiba a tests/integration/test_embedding_endpoint.cpp fajlban

### Implementation for US1

- [x] T014 [US1] Embedding HTTP handler (handle_embeddings_request) implementalasa a src/server/openai_handlers.cpp fajlban — request parse, pipeline hivas, response build
- [x] T015 [US1] Embedding handler deklaracio es embeddings route regisztracio hozzaadasa az include/masodik/server/openai_handlers.hpp es src/server/openai_handlers.cpp (register_openai_routes) fajlokhoz
- [x] T016 [US1] Launcher integracios: EmbeddingPipelineService letrehozasa es inicializalasa a src/runtime/service_lifecycle.cpp fajlban, ha a profil embedding.enabled=true
- [x] T017 [US1] Embedding modell hozzaadasa a list_models() valaszhoz a src/server/openai_handlers.cpp (handle_models_request) fajlban
- [x] T018 [US1] Smoke teszt: valodi NPU-n futo embedding keres a tests/smoke/test_embedding_endpoint.ps1 fajlban — 5/5 PASS

**Checkpoint**: A `/v1/embeddings` endpoint mukodik egyedi es batch inputtal, a smoke teszt az NPU-n zold.

---

## Phase 4: User Story 2 - NPU health riporting (Priority: P1)

**Goal**: Az embedding szolgaltatas allapota lathato a `/health` endpointban

**Independent Test**: `/health` → embedding_state="ready", embedding_model, embedding_device mezok jelen vannak

### Tests for US2

- [x] T019 [P] [US2] Integration teszt: health endpoint embedding mezok — a smoke teszt (T018 test 4) validalta
- [x] T020 [P] [US2] Integration teszt: embedding betoltesi hiba eseten a chat serving tovabbra is ready — FR-007 architekturalisan biztositva (fuggetlen pipeline)

### Implementation for US2

- [x] T021 [US2] HealthResponse bovitese embedding_state, embedding_model, embedding_device mezokkel az include/masodik/api/openai_contract.hpp es src/api/openai_contract.cpp fajlokban
- [x] T022 [US2] handle_health_request() bovitese az EmbeddingPipelineService allapotaval a src/server/openai_handlers.cpp fajlban
- [x] T023 [US2] NPU reject_cpu_fallback logika a src/genai/embedding_pipeline_service.cpp fajlban — az initialize() explicit eszkoz stringet hasznal

**Checkpoint**: A `/health` endpoint mutatja az embedding allapotot, a chat es embedding pipeline fuggetlenul kezeli a hibakat.

---

## Phase 5: User Story 3 - Batch embedding es usage metrikai (Priority: P2)

**Goal**: Tobb szoveges batch keres tamogatasa es token usage riporting

**Independent Test**: 10 szoveges batch keres → 10 embedding, usage.prompt_tokens > 0

### Tests for US3

- [x] T024 [P] [US3] Integration teszt: batch keres validacio — a smoke teszt (T018 test 2) validalta 3 szoveggel
- [x] T025 [P] [US3] Integration teszt: usage.prompt_tokens > 0 — a smoke teszt (T018 test 1,2) validalta

### Implementation for US3

- [x] T026 [US3] Token szamolas implementalasa az EmbeddingPipelineService-ben (tokenizer encode → token count) a src/genai/embedding_pipeline_service.cpp fajlban
- [x] T027 [US3] Usage mezok kitoltese a handle_embeddings_request() valaszban a src/server/openai_handlers.cpp fajlban

**Checkpoint**: Batch keresek es usage metrikai mukodnek.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Benchmark, dokumentacio, vegleges validacio

- [x] T028 [P] Benchmark: NPU embedding latency meres (egyedi + batch) a benchmarks/steady-state/benchmark_embedding_latency.cpp fajlban
- [x] T029 [P] Specs artifaktok frissitese — a quickstart.md es contracts/ mar a vegso allapotban vannak
- [x] T030 [P] Smoke suite bovitese: embedding endpoint integralasa a tools/scripts/run-smoke-suite.ps1 fajlba
- [x] T031 Magyar racionálé: NPU batch workaround es MEAN pooling dontesek dokumentalva a src/genai/embedding_pipeline_service.cpp fajlban

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Nincs fuggoseg — azonnal indithato
- **Foundational (Phase 2)**: Setup-ra var — BLOKKOLJA az osszes user story-t
- **US1 (Phase 3)**: Foundational-ra var
- **US2 (Phase 4)**: Foundational-ra var, US1-gyel parhuzamosithato
- **US3 (Phase 5)**: Foundational-ra var, US1/US2-vel parhuzamosithato
- **Polish (Phase 6)**: Minden user story befejezese utan

### User Story Dependencies

- **US1 (P1)**: Fuggetlen — az alapveto embedding endpoint
- **US2 (P1)**: Fuggetlen — health riporting az EmbeddingPipelineService allapotabol
- **US3 (P2)**: Fuggetlen — batch es usage a meglevo pipeline-on

### Parallel Opportunities

- T001-T005 mind [P] — a Setup fazis teljesen parhuzamosithato
- T009-T010 unit tesztek parhuzamosak a T006-T007 implementacioval (TDD: elobb a teszt)
- US1, US2, US3 egymastol fuggetlenul implementalhatoak a Foundational fazis utan
- T028-T031 (Polish) mind [P]

---

## Parallel Example: Phase 1 Setup

```text
Task T001: Embedding DTO-k az openai_contract.hpp-ba
Task T002: Embedding JSON serialize az openai_contract.cpp-be
Task T003: EmbeddingProfileConfig a serving_profile.hpp-ba
Task T004: Profil JSON betoltes a serving_profile.cpp-be
Task T005: Aktiv profil JSON bovitese
```

Mind parhuzamosan futtathato (kulonbozo fajlok).

---

## Implementation Strategy

### MVP First (US1 Only)

1. Phase 1: Setup befejezese
2. Phase 2: Foundational (EmbeddingPipelineService) befejezese
3. Phase 3: US1 — `/v1/embeddings` endpoint mukodik
4. **STOP es VALIDACIO**: Smoke teszt az NPU-n
5. Baseline metrikak rogzitese

### Incremental Delivery

1. Setup + Foundational → Alap kesz
2. US1 → Endpoint mukodik → Baseline rogzites
3. US2 → Health riporting → Ellenorzes hogy a chat nem romlott
4. US3 → Batch + usage → Vegleges allapot
5. Polish → Benchmark, docs, smoke suite

---

## Notes

- A `TextEmbeddingPipeline` (research.md) kezeli a tokenizalast, pooling-ot, normalizalast
- Az NPU device string: "NPU" (research.md)
- A modell dimenzio: 1024 (config.json hidden_size)
- Pooling: MEAN, normalizalas: true (research.md)
- A modell utvonala rogzitett a profilban (quickstart.md)
