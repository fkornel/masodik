# Data Model: Manual Model Split Serving

## 1. Split Model Artifact Set

**Leiras**: A gpt-oss-20b ket osszetarto modellreszbol allo artifact-keszlete, amely a split topology statikus szerzodeset rogzit.

### Fo mezok

| Mezo | Tipus | Leiras |
|------|------|--------|
| `model_name` | szoveg | A logikai modellazonosito |
| `artifact_version` | szoveg | A split artifact valtozatjelzese |
| `source_model_checksum` | szoveg | A forras monolitikus IR lenyomata a reprodukalhatosaghoz |
| `stage_count` | szam | A stage-ek szama, jelen feature-ben fixen 2 |
| `stage_boundary` | szoveg | A reteghatar emberileg ertelmezheto leirasa |
| `cut_after_layer` | szoveg | A konkret vagas, pl. `layers.17 -> layers.18` |
| `stage_1_path` | fajlut | Az elso stage artifact helye |
| `stage_2_path` | fajlut | A masodik stage artifact helye |
| `tokenizer_path` | fajlut | A kozos tokenizer helye |
| `max_context_tokens` | szam | A validalt maximalis kontextusablak |
| `hidden_state_contract` | azonosito | A ket stage kozti handoff kompatibilitasi lenyomata |
| `kv_contract` | azonosito | A KV-cache ownership es forma szerzodese |
| `stage_1_layer_range` | szoveg | A Stage-1 retegtartomanya, kezdetben kb. `0-17` |
| `stage_2_layer_range` | szoveg | A Stage-2 retegtartomanya, kezdetben kb. `18-43` |
| `status` | allapot | `draft`, `validated`, `invalid`, `deprecated` |

### Validacios szabalyok

- `stage_count` csak `2` lehet az elso verzios feature-ben.
- `stage_1_path` es `stage_2_path` kotelezo.
- A `hidden_state_contract` es a `kv_contract` egyezese kotelezo a ket stage kozott.
- A `cut_after_layer`, `stage_1_layer_range` es `stage_2_layer_range` nem mondhatnak ellent egymasnak.
- `status=validated` csak akkor lehet, ha a stage-kompatibilitas-ellenorzes es a warmup-lanc sikeres.

## 2. Execution Stage

**Leiras**: A split serving egyik fix futasi szakasza, kulon betoltesi, warmup es diagnosztikai allapottal.

### Fo mezok

| Mezo | Tipus | Leiras |
|------|------|--------|
| `stage_id` | azonosito | `stage-1` vagy `stage-2` |
| `device_id` | szoveg | A hozzarendelt gyorsito |
| `artifact_path` | fajlut | A stage sajat modellreszenek helye |
| `layer_range` | szoveg | A stage-hez tartozo retegtartomany |
| `load_state` | allapot | `uninitialized`, `loading`, `ready`, `failed` |
| `warm_state` | allapot | `not_started`, `warming`, `warmed`, `failed` |
| `memory_budget_gb` | szam | A stage uzemeltetesi memoria-kerete |
| `kv_budget_gb` | szam | A stage-re tervezett KV-cache keret |
| `last_error_code` | szoveg | Az utolso ismert hiba kategoriaja |
| `last_error_message` | szoveg | Emberileg ertelmezheto hibaosszegzes |
| `last_ready_at` | idobelyeg | Az utolso sikeres ready allapot ideje |

### Allapotatmenetek

`uninitialized -> loading -> ready`  
`loading -> failed`  
`ready -> failed`  
`ready -> warming -> warmed`  
`warming -> failed`

### Gepspecifikus kezdeti cel

- `stage-1`: `GPU.1`, kb. `0-17` reteg, kb. `5.5 GB` suly + kb. `3.5 GB` KV/scratch budget
- `stage-2`: `GPU.0`, kb. `18-43` reteg, kb. `7.0 GB` suly + kb. `15-20 GB` KV budget

## 3. Inter-Stage Handoff

**Leiras**: Keresenkenti adatobjektum, amely az elso stage kimenetet a masodik stage bemenetekent hordozza.

### Fo mezok

| Mezo | Tipus | Leiras |
|------|------|--------|
| `request_id` | azonosito | A keres egyedi azonositoja |
| `sequence_id` | azonosito | A generacios szekvencia azonositoja |
| `hidden_state_shape` | leiro adat | A tenzor vart alakja |
| `precision_profile` | szoveg | A handoff numerikus pontossagi profilja |
| `token_span` | leiro adat | A handoff altal fedett tokenablak |
| `transport_medium` | szoveg | A Stage-1 -> Stage-2 atadas utja, kezdetben RAM-on keresztuli tenzoratadas |
| `handoff_state` | allapot | `created`, `transferred`, `consumed`, `failed`, `cancelled` |
| `producer_stage_id` | azonosito | Az eloallito stage |
| `consumer_stage_id` | azonosito | A fogyaszto stage |
| `created_at` | idobelyeg | Letrehozas ideje |
| `completed_at` | idobelyeg | Lezaras ideje |

### Validacios szabalyok

- Egy `request_id`-hez egyszerre csak a sajat handoff objektum tartozhat.
- `transferred` allapot elott a `consumer_stage_id` kotelezo.
- `consumed` allapot csak `transferred` utan engedelyezett.
- A `transport_medium` nem lehet "implicit runtime fallback"; az atadas modjat expliciten dokumentalni kell.

## 4. Split Serving Profile

**Leiras**: A serving konfiguracio logikai nezetben, amely a ket stage topologyjat es az uzemeltetesi korlatokat definialja.

### Fo mezok

| Mezo | Tipus | Leiras |
|------|------|--------|
| `profile_name` | szoveg | Az aktiv split profil neve |
| `bind_host` | szoveg | Hallgatozasi cim |
| `bind_port` | szam | Hallgatozasi port |
| `stage_1_device` | szoveg | Elso stage gyorsito |
| `stage_2_device` | szoveg | Masodik stage gyorsito |
| `reject_cpu_fallback` | logikai | Tiltott-e barmilyen CPU-visszaeses |
| `max_context_tokens` | szam | Validalt kontextuslimit |
| `stage_1_target_layer_ratio` | szam | Kezdetben kb. `0.40-0.45` |
| `request_path_mode` | szoveg | `placeholder`, `compiled`, `degraded` |
| `max_concurrent_requests` | szam | Megengedett egyideju keresek |
| `queue_limit` | szam | Varolistara engedett keresesek |
| `diagnostics_level` | szoveg | A split diagnosztika reszletessege |

### Validacios szabalyok

- `stage_1_device` es `stage_2_device` nem lehet azonos.
- `reject_cpu_fallback=true` kotelezo a split profil MVP-jeben.
- A profil csak validated artifact setre mutathat.
- `request_path_mode=compiled` a feature lezaro allapotanak kotelezo resze.

## 5. Stage Diagnostic Snapshot

**Leiras**: Operatori es health celra szolgalo pillanatfelvetel a ket-stage lanc allapotarol.

### Fo mezok

| Mezo | Tipus | Leiras |
|------|------|--------|
| `snapshot_time` | idobelyeg | A mintavetel ideje |
| `service_state` | allapot | `starting`, `ready`, `degraded`, `failed` |
| `stage_1_state` | allapot | Az elso stage aktualis allapota |
| `stage_2_state` | allapot | A masodik stage aktualis allapota |
| `handoff_state` | allapot | A legutobbi handoff allapota |
| `memory_summary` | osszegzes | Stage-enkenti es teljes memoriahelyzet |
| `last_request_outcome` | szoveg | Az utolso befejezett keres rovid eredmenye |
| `last_failure_scope` | szoveg | `stage-1`, `stage-2`, `handoff`, `startup`, vagy ures |
| `request_path_mode` | szoveg | Tenylegesen placeholder vagy compiled split request-ut futott-e |
| `validated_context_tokens` | szam | Az artifact/profil altal igazolt context ceiling |

### Hasznalat

- `/health` szerzodes bovitesenek alapja
- uzemeltetesi naplok strukturalt forrasa
- benchmark-futasok utolagos korrelacios pontja
- a ket-GPU-s proof bundle egyik osszefoglalo allapota
