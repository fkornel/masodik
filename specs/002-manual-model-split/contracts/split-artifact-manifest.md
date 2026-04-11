# Contract: Split Artifact Manifest

## Cel

Ez a contract rogziti, milyen minimalis szerzodes szerint adhat at egy split-artifact eloallito folyamat ket stage-re bontott modellkeszletet a serving rendszernek.

## Formatum

JSON dokumentum a split artifact gyokerkonyvtaraban.

## Kotelezo mezoelvarasok

| Mezo | Kotelezo | Leiras |
|------|----------|--------|
| `model_name` | igen | A logikai modellazonosito |
| `artifact_version` | igen | A split artifact verzioja |
| `source_model_checksum` | igen | A forras monolitikus IR lenyomata a reprodukalhatosaghoz |
| `stage_count` | igen | A stage-ek szama, jelenleg `2` |
| `stage_boundary` | igen | Emberileg ertelmezheto reteghatar-leiras |
| `cut_after_layer` | igen | A konkret vagas, pl. `layers.17 -> layers.18` |
| `stage_1` | igen | Az elso stage leiro objektuma |
| `stage_2` | igen | A masodik stage leiro objektuma |
| `tokenizer_path` | igen | A kozos tokenizer helye |
| `hidden_state_contract` | igen | A ket stage kozti input/output kompatibilitasi azonosito |
| `kv_contract` | igen | A KV-cache szerzodes azonositoja |
| `validated_for_profile` | igen | A profil, amelyre a split artifact hitelesitett |
| `validated_context_tokens` | igen | A split artifacttal hitelesitett context ceiling |

## Stage objektum minimalis elvarasa

| Mezo | Kotelezo | Leiras |
|------|----------|--------|
| `device_role` | igen | `primary` vagy `secondary` |
| `artifact_path` | igen | A stage modellreszenek helye |
| `layer_range` | igen | A stage retegtartomanya, pl. `0-17` vagy `18-43` |
| `expected_input` | igen | A stage vart bemeneti szerzodesenek leirasa |
| `expected_output` | igen | A stage kimeneti szerzodesenek leirasa |
| `memory_budget_gb` | igen | Javasolt stage memoria-budget |
| `kv_budget_gb` | igen | A stage sajat KV-cache budgetje |

## Pelda

```json
{
  "model_name": "gpt-oss-20b-int4-ov",
  "artifact_version": "split-v1",
  "source_model_checksum": "sha256:...",
  "stage_count": 2,
  "stage_boundary": "transformer layers 0-17 | 18-43",
  "cut_after_layer": "layers.17 -> layers.18",
  "tokenizer_path": "tokenizer",
  "hidden_state_contract": "hs-v1",
  "kv_contract": "kv-v1",
  "validated_for_profile": "b580-manual-split",
  "validated_context_tokens": 131072,
  "stage_1": {
    "device_role": "primary",
    "artifact_path": "stage-1",
    "layer_range": "0-17",
    "expected_input": "token ids + kv context",
    "expected_output": "hidden states + kv delta",
    "memory_budget_gb": 12,
    "kv_budget_gb": 3.5
  },
  "stage_2": {
    "device_role": "secondary",
    "artifact_path": "stage-2",
    "layer_range": "18-43",
    "expected_input": "hidden states + kv delta",
    "expected_output": "logits + updated kv state",
    "memory_budget_gb": 24,
    "kv_budget_gb": 18
  }
}
```

## Validacios szabaly

1. `stage_count` mas erteke eseten a serving startup hibaval megall.
2. A `hidden_state_contract` es `kv_contract` hianya hard failure.
3. A `validated_for_profile` nem egyezhet tetszoleges profillal; csak olyan profillal indulhat a serving, amelyet a manifest explicit tamogat.
4. A `cut_after_layer`, `stage_boundary` es a stage `layer_range` mezoknek egymassal konzisztensnek kell lenniuk.
5. A `validated_context_tokens` csak akkor tekintheto ervenyesnek, ha a manifesthez tartozo proof kor tenyleges ket-GPU-s compiled request-uttal keszult.

## Contract-to-code traceability

| Contract elem | Kodhely | Megjegyzes |
|---------------|---------|------------|
| `split-manifest.json` felderites | `src\config\model_repository.cpp` | A model root alatt keresi a manifestet, es split modban hard failure a hianya |
| Manifest DTO es field projection | `include\masodik\config\model_repository.hpp`, `src\config\model_repository.cpp` | Itt materializalodik a `stage_count`, `stage_1`, `stage_2`, `hidden_state_contract`, `kv_contract`, `validated_for_profile` |
| Profil-manifest paritas | `src\runtime\service_config.cpp`, `src\runtime\service_dependency_validator.cpp` | A `validated_for_profile` es a split topology startup-gate-et kap |
| Stage artifact feloldas | `src\genai\llm_pipeline_service.cpp` | A `build_split_startup_plan()` a manifest stage-utvonalait konkret stage-1/stage-2 artifact pathra oldja |
| Stage-enkenti IR loader | `src\genai\llm_pipeline_service.cpp` | Az `initialize_split_stage()` a manifest altal mutatott stage konyvtarbol `openvino_model.xml`-t compile-ol |
| Warmup es startup diagnosztika | `src\genai\llm_pipeline_service.cpp`, `docs\local-startup.md` | A manual split startup csak ket kulon GPU es ervenyes split artifact mellett lehet ready |

## Jelenlegi validacios hatar

- Ez a contract ma **startup- es stage-loader szinten** enforce-olt.
- A manifest hitelesitett stage-1/stage-2 IR-eket, kulon device-hozzarendelest es warmup-ot fed le.
- A valodi request-time tensor chaining es a ket stage kozti tenyleges hidden-state atadas kulon runtime-validacios gate; egy monolitikus AI Playground modellkonyvtar **nem** eleg a contract teljesitesere.
- A feature vegere a contractnak mar a forrasmodell-checksumot, a konkret vagaspontot es a 128k-validacio bizonyitasat is hordoznia kell.
