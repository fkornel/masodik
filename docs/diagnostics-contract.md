# Diagnostics Contract

## Health payload

A `/health` válaszban a következő mezőknek kell látszaniuk:

- `ready`
- `state`
- `active_model`
- `active_profile`
- `effective_device`
- `queue_depth`
- `gpu_offload_active`

## Runtime hiba jelentés

A surfaced runtime hibák célja, hogy különbséget tegyenek:

- konfigurációs hiba
- toolchain / runtime hiány
- modellhiány
- degradált hybrid állapot

## Queue és degradáció

- Ha kérés sorba kerül, a `queue_depth` ezt mutassa.
- Ha a hybrid gyorsítás nem aktív, de a profil ezt kéri, a `state` ne maradjon félrevezetően `ready`.
- A diagnosztika ne fedje el a GPU-only és HETERO út közti különbséget.

## Streaming diagnosztika és hibafelület

- A stream megnyitasa elotti hibak JSON hibavalaszkent jelenjenek meg, ugyanazzal a hibakoddal,
  mint a nem streamelt kereseseknel.
- A mar elindult stream kozbeni hibak SSE terminalis hibaesemenykent jelenjenek meg, hogy a kliens
  ne csendes EOF-bol vagy vegtelen varakozasbol kovetkeztessen a hibara.
- A runtime metrikanak kulon primitivekben kovetnie kell az aktiv streamek szamat, a streamhibak
  darabszamat es az utolso disconnect socket hibat.
- A streaming diagnosztika alapretege nem rejtheti el, hogy a hiba a stream elott vagy a stream
  kozben tortent.

## Tool calling diagnosztika

- A tool-callinghoz kotheto validacios hibak kulon `tool_validation_error` kodon jelenhetnek meg,
  hogy a kliens meg tudja kulonboztetni az altalanos JSON hibaktol.
- A mar megnyitott tool-call stream kozbeni hibak kulon `tool_stream_error` kodon is felszinre
  hozhatok, ha a route ismeri a kereskontextust.
- A belso diagnosztikai primitiveknek kulon szamlalniuk kell:
  - a sikeresen visszaadott tool call valaszokat
  - a streamelt tool-call delta kibocsatasok darabszamat
  - a tool definicios validacios hibakat
- A Phase 13 alapozasa utan ezek a primitivek mar elerhetok, akkor is, ha a teljes health-szerzodesi
  surfacelesuk csak a kovetkezo fazisokban tortenik meg.

## Request audit diagnosztika

- A request audit logbejegyzeseknek strukturalt `request_audit {json}` alakban kell megjelenniuk,
  hogy a smoke es regresszios helper egyszeruen tudja keresni oket.
- A request audit rekord legalabb a kovetkezo mezoket **MUST** tartalmazza:
  - `request_id`
  - `stream`
  - `tools_present`
  - `tool_count`
  - `tool_choice_kind`
  - `tool_choice_name`
  - `decision_path`
  - `validation_outcome`
- A `decision_path` ervenyes alapertekei:
  - `unknown`
  - `content_only`
  - `tool_call_auto`
  - `tool_call_required`
  - `tool_call_named`
- A `validation_outcome` foundational szintjen legalabb a kovetkezo osztalyoknak kell letezniuk:
  - `not_evaluated`
  - `accepted`
  - `parser_error`
  - `tool_schema_error`
  - `model_selection_error`
  - `runtime_readiness_error`
  - `runtime_error`
- A request audit logger **MUST NOT** teljes promptdumpot irni alapertelmezetten; legfeljebb roviditett
  message preview jelenhet meg.
- A request audit rekordot a kliensoldali `X-Request-Id` fejleccel **MUST** lehessen korrelalni.

### Minta logbejegyzes

```text
request_audit {"request_id":"chatreq-42","stream":true,"tools_present":true,"tool_count":1,"tool_choice_kind":"auto","tool_choice_name":"","decision_path":"tool_call_auto","validation_outcome":"accepted","truncated_message_preview":"Deritsd ki a nyitott PR-eket egy elerheto toollal.","created_at":"2026-04-04T22:00:00Z"}
```

### Mezoertelmezes

- `tools_present = true` es `tool_count > 0`: a kliens tenylegesen kuldott tool definiciot
- `tool_choice_kind = "absent"`: a kliens nem kuldott `tool_choice` mezot
- `tool_choice_kind = "auto"` + `decision_path = "content_only"`: a kliens kuldott toolokat, de a szerver
  nem valtott tool-call agba
- `tool_choice_kind = "required"` + `decision_path = "tool_call_required"`: a valasz kotelezo tool-call agra ment
- `tool_choice_kind = "named_function"` + `validation_outcome = "tool_schema_error"`: a named tool nem
  egyezett az ervenyes `tools[]` listaval

## Split stage diagnosztika

A manual split modban a diagnosztikai reteg kulon stage- es handoff-szintu
allapotot, hiba-scope-ot es memoria-osszefoglalot ad.

### Split health payload mezok

| Mezo | Tipus | Leiras |
|------|-------|--------|
| `split_mode` | bool | A pipeline split modban fut-e |
| `split_profile` | string | Az aktiv split profil neve |
| `stage_1_state` | string | A stage-1 (dGPU) allapota: `uninitialized`, `starting`, `ready`, `degraded`, `failed` |
| `stage_2_state` | string | A stage-2 (iGPU) allapota: ugyanazok az ervenyes allapotok |
| `handoff_state` | string | A handoff allapot: `idle`, `created`, `ready`, `consumed`, `failed`, `backpressured`, `cancelled` |
| `last_failure_scope` | string | Az utolso hiba hatokore: `startup`, `stage-1`, `stage-2`, `handoff`, vagy ures |
| `fallback_detected` | bool | CPU vagy azonos-device fallback eszleles tortent-e |
| `stage_1_execution_devices` | string | Az OpenVINO altal Stage-1-hez tenylegesen visszaadott execution device lista |
| `stage_2_execution_devices` | string | Az OpenVINO altal Stage-2-hoz tenylegesen visszaadott execution device lista |
| `last_split_request_id` | string | Az utolso compiled split keres runtime azonositoja |
| `last_split_sequence_id` | string | Az utolso compiled split handoff szekvenciaazonositoja |
| `memory_summary` | object | Stage-enkenti es rendszerszintu memoria-budget osszefoglalas |
| `last_failure_summary` | string | Strukturalt egysoros osszefoglalas az utolso hibarol |

### Oszinte health szemantika

- Ha barmelyik stage `failed` allapotban van, a `ready` mezo **MUST** `false` legyen.
- Ha `fallback_detected = true`, a `ready` **MUST** `false` legyen.
- A `state` mezo `failed` kell legyen, ha barmelyik stage nem `ready`.
- A health endpoint soha nem hazudhat `ready`-t, ha a split pipeline nem mukodokepes.

### Failure scope osztalyozas

| Scope | Mikor | Peldak |
|-------|-------|--------|
| `startup` | A pipeline inicializacio soran | CPU device, azonos device, manifest hiba |
| `stage-1` | A stage-1 betoltese/futasa soran | dGPU hiba, modell betoltesi hiba |
| `stage-2` | A stage-2 betoltese/futasa soran | iGPU hiba, VRAM tulcsordulas |
| `handoff` | A stage-1 → stage-2 atadasnal | Handoff lease hiba, timeout |

### Strukturalt split log formatum

```text
split_stage stage_id=stage-1 state=ready device=GPU.1 memory_state=budget:12.0GB
split_stage stage_id=stage-2 state=ready device=GPU.0 memory_state=budget:24.0GB
split_failure_summary scope=stage-2 stage_1=ready stage_2=failed handoff=failed fallback=false error="CL_OUT_OF_RESOURCE"
manual_split_proof request_id=splitreq-17 sequence_id=seq-17 request_path_mode=compiled handoff_state=consumed stage_1_device=GPU.1 stage_2_device=GPU.0 stage_1_execution_devices="GPU.1" stage_2_execution_devices="GPU.0" split_artifact_version=split-v1 validated_context_tokens=131072
```

### Runtime hiba report bovites

A split runtime hibak report-ja a kovetkezo plusz mezokkel egeszul ki:

- `Split scope`: a hiba hatokore
- `Stage-1` / `Stage-2`: az egyes stage-ek allapota
- `Handoff`: a handoff allapot
- `Fallback`: igen/nem
- `Utolso hiba`: az utolso releváns hibauzenet reszlete

### Contract-to-code traceability

| Felulet | Kodhely | Megjegyzes |
|---------|---------|------------|
| Split health payload | `include\masodik\api\openai_contract.hpp`, `src\api\openai_contract.cpp`, `src\server\openai_handlers.cpp` | A kulso `/health` szerzodes JSON surfacelese |
| Split diagnostic snapshot | `include\masodik\runtime\health_snapshot.hpp`, `src\runtime\readiness_reporter.cpp`, `src\genai\llm_pipeline_service.cpp` | A belso runtime allapotokbol epulo operatori snapshot |
| Failure scope mapping | `include\masodik\runtime\error_category.hpp`, `src\runtime\error_category.cpp`, `src\genai\llm_pipeline_service.cpp` | Startup / stage-1 / stage-2 / handoff scope osztalyozas |
| Strukturalt split logok | `include\masodik\diagnostics\logger.hpp`, `src\diagnostics\logger.cpp`, `src\diagnostics\runtime_error_reporter.cpp` | `split_stage`, `split_failure_summary`, `manual_split_runtime_failure`, `manual_split_proof` |
| Smoke es quickstart gate | `tests\smoke\test_manual_split_startup.ps1`, `tests\smoke\test_manual_split_long_context.ps1`, `tests\smoke\test_manual_split_diagnostics.ps1`, `tools\scripts\run-smoke-suite.ps1` | A placeholder es a hianyzo split artifact mar nem adhat hamis PASS-t |

### Validacios megjegyzes

- A diagnosztikai szerzodes **nem helyettesiti** a valodi ketstage-es serving bizonyitasat.
- Ha a manual split route csak placeholder valaszt ad, akkor a health es log lathatosag ugyan hasznos, de a feature nincs kesz.
- Ezert a Phase 6 smoke/benchmark kor most mar explicit elutasitja a `Placeholder valasz` tartalmat, hogy a dGPU+iGPU split-validacio ne tunhessen kesznek valodi chaining nelkul.
- A Phase 9 proof workflow kulon `capture-manual-split-proof.ps1` szkripttel mintavetelezi a launcher process GPU Process Memory es GPU Engine szamlaloit, hogy a Task Managerben vart ketadapteres aktivitet kulon proof artifactban is rogzitse.

