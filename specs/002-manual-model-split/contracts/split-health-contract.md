# Contract: Split Health Payload

## Cel

Ez a contract rogziti a split profil uzemeltetesi health valasz minimalis boviteseit, hogy a ket-stage serving allapota kivulrol is egyertelmuen ellenorizheto legyen.

## Erintett felulet

A meglvo health endpoint valaszaba illeszkedo strukturalt statusz-mezok.

## Kotelezo split mezok

| Mezo | Tipus | Leiras |
|------|------|--------|
| `split_mode` | logikai | Jelzi, hogy a szolgaltatas explicit split modban fut-e |
| `split_profile` | szoveg | Az aktiv split profil neve |
| `stage_1_state` | szoveg | `starting`, `ready`, `degraded`, `failed` |
| `stage_2_state` | szoveg | `starting`, `ready`, `degraded`, `failed` |
| `handoff_state` | szoveg | `idle`, `ready`, `failed`, `backpressured` |
| `request_path_mode` | szoveg | `compiled`, `placeholder`, `degraded` |
| `stage_1_device` | szoveg | A valos stage-1 device azonosito |
| `stage_2_device` | szoveg | A valos stage-2 device azonosito |
| `split_artifact_version` | szoveg | Az aktiv split artifact verzioja |
| `validated_context_tokens` | szam | A jelen profilhoz igazolt context ceiling |
| `last_failure_scope` | szoveg | `stage-1`, `stage-2`, `handoff`, `startup`, vagy ures |
| `fallback_detected` | logikai | Tortent-e nem vart visszaeses |
| `memory_summary` | objektum | Stage-enkenti es globalis memoria-osszefoglalo |
| `last_failure_summary` | szoveg | Strukturalt, rovid operatori osszefoglalo az utolso split hibarol |

## Pelda valasz

```json
{
  "ready": true,
  "state": "ready",
  "status_message": "A split pipeline keszen all.",
  "split_mode": true,
  "split_profile": "b580-manual-split",
  "stage_1_state": "ready",
  "stage_2_state": "ready",
  "handoff_state": "ready",
  "request_path_mode": "compiled",
  "stage_1_device": "GPU.1",
  "stage_2_device": "GPU.0",
  "split_artifact_version": "split-v1",
  "validated_context_tokens": 131072,
  "last_failure_scope": "",
  "fallback_detected": false,
  "memory_summary": {
    "stage_1": "healthy",
    "stage_2": "healthy",
    "system": "healthy"
  },
  "last_failure_summary": ""
}
```

## Szabalyszeru elvarasok

1. `ready=true` csak akkor engedelyezett, ha mindket stage `ready`, a `handoff_state` nem `failed`, es `fallback_detected=false`.
2. Barmelyik stage failure-je eseten a globalis `state` nem maradhat `ready`.
3. Ha a rendszer egyetlen gyorsitora vagy CPU-ra esik vissza, a `fallback_detected` kotelezoen `true`.
4. A feature lezaro allapotaban `request_path_mode` nem lehet `placeholder`.
5. A `stage_1_device` es `stage_2_device` nem lehet azonos, es nem utalhat CPU device-ra.

## Contract-to-code traceability

| Contract elem | Kodhely | Megjegyzes |
|---------------|---------|------------|
| Health DTO | `include\masodik\api\openai_contract.hpp` | Itt definialt a split-aware `HealthResponse` |
| JSON serialization | `src\api\openai_contract.cpp` | A split field-ek es a `last_failure_summary` itt surfacelodnak `/health` JSON-be |
| Snapshot assembly | `include\masodik\runtime\health_snapshot.hpp`, `src\runtime\readiness_reporter.cpp` | A runtime snapshotbol epul fel a split health payload |
| Runtime state projection | `src\genai\llm_pipeline_service.cpp` | A stage allapot, handoff, fallback, failure scope es memory summary innen kerul a healthbe |
| HTTP surfaceles | `src\server\openai_handlers.cpp` | A `/health` route a pipeline-service response-t adja vissza |
| Smoke validacio | `tests\smoke\test_manual_split_diagnostics.ps1`, `tests\smoke\test_manual_split_long_context.ps1` | A split field-eket es a nem-placeholder request utat validaljak |

## Jelenlegi validacios hatar

- A health payload mar oszinten mutatja a split startup allapotot, a stage statuszt es a fallback-detectet.
- A `ready=true` nem tekintheto elegsegesnek valodi manual split serving bizonyiteknak, ha a request ut placeholder valaszt ad.
- Emiatt a Phase 6 validacios kor kulon tiltja a `Placeholder valasz` tartalmat a manual split smoke- es benchmark-utakban.
- A feature vegere a health payloadnak operatori szinten is kulon lathatova kell tennie, hogy a valasz compiled split request-utrol jott-e, melyik artifact-verzioval, es milyen context ceiling mellett.
