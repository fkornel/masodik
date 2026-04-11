# Hibaelhárítás

## Startup `failed` állapot

- Ha a hiba `missing_model`, ellenőrizd, hogy a helyi modellrepo és az `openvino_model.xml` / `openvino_model.bin` fájlok elérhetők.
- Ha a hiba `missing_toolchain`, ellenőrizd az OpenVINO / GenAI bundle DLL-jeit és a CMake hint útvonalakat.
- Ha a hiba `invalid_configuration`, ellenőrizd a profil bind host, bind port, device és timeout mezőit.

## Restart és leállítás

- A launcher leállítása explicit folyamatleállítással vagy konzoljelzéssel történjen.
- Újraindítás előtt a korábbi folyamatnak teljesen le kell állnia, különben a bind port foglaltnak látszhat.

## Hibrid profil

- A `HETERO:GPU.0,CPU` profil nem teljesítménygarancia, hanem működőképességi út.
- Ha a kívánt hibrid profil nem tartható, a runtime `failed` vagy `degraded` állapottal jelezzen, ne csendes placeholder móddal.

## Streaming hibák és kliensabort

- Ha a `stream: true` kérés már a stream megnyitása előtt bukik el, a kliensnek normál JSON hibaválaszt kell kapnia, nem félbehagyott SSE kapcsolatot.
- Ha a stream már elindult, de a generálás közben hiba történik, a kliensnek terminális SSE hibaeseményt és utána `data: [DONE]` lezárást kell látnia.
- Ha a kliens idő előtt bontja a kapcsolatot, a szerveroldali kéréslease-nek és aktív stream számlálónak felszabadulva kell maradnia; az ezt követő kérés nem akadhat be csendben.
- A stream abort reprodukálására használd a `tests\smoke\test_stream_abort.ps1` szkriptet.

## Tool calling hibák és korlatok

- Ha a kliens hibas `tools` vagy `tool_choice` payloadot kuld, a szerver `tool_validation_error` kodot adhat vissza.
- Ha a tool-call stream mar elindult, de utkozben hibara fut, a terminális SSE hiba `tool_stream_error` kodon is megjelenhet.
- A jelenlegi tool-calling MVP nem hajt vegre toolt. A valasz csak a strukturalt `tool_calls` payloadot adja vissza, a tenyleges futtatast a kliensoldali orchestratornak kell kezelnie.
- A jelenlegi modell/runtime korlatok mellett a tool-call szerkezet determinisztikus szerveroldali mappinggel is kialakulhat; ez kompatibilitasi stabilitast ad, nem szerveroldali tool-executiont.

## Request audit log ertelmezese

- A chat valaszok `X-Request-Id` fejlece alapjan keresd vissza a megfelelo `request_audit {json}` sort a
  `build\logs\masodik-runtime.log` fajlban.
- Ha `tools_present = false`, akkor a kliens nem kuldott `tools[]` tombot.
- Ha `tools_present = true`, de `decision_path = "content_only"`, akkor a kliens kuldott tool listat, de a
  szerver nem valasztott tool-call agat.
- Ha `tool_choice_kind = "required"` es `decision_path = "tool_call_required"`, akkor a kereskontextus
  tenylegesen kotelezett tool valasszal lett kiszolgalva.
- Ha `tool_choice_kind = "named_function"` es `validation_outcome = "tool_schema_error"`, akkor a named
  tool nincs jelen a request `tools[]` listajaban vagy schema szerint ervenytelen.
- Ha `validation_outcome = "parser_error"`, akkor a hiba mar a request JSON vagy az OpenAI contract parse
  szintjen tortent.

### Gyors diagnosztikai parancsok

```powershell
Select-String -Path build\logs\masodik-runtime.log -Pattern "request_audit"
.\tests\smoke\test_request_audit_logging.ps1 -BaseUrl http://127.0.0.1:29000
```

## Manual split hibak

- Ha a startup `split_manifest_invalid` hibaval all meg, ellenorizd a `split-manifest.json` fajlt, a
  `validated_for_profile` mezot, valamint a stage-1/stage-2 artifact utvonalakat.
- Ha a health `fallback_detected = true` allapotot mutatna, a split profil uzemszeruen hibasnak tekintendo;
  a Phase 2-tol kezdve ez nem engedelyezett "best effort" visszaeses.
- Ha a health `last_failure_scope` mezoje `stage-1` vagy `stage-2`, eloszor az adott stage artifact es device
  hozzarendeleset ellenorizd.
- Ha a `last_failure_scope = "handoff"`, akkor a hidden-state szerzodes, a stage-hatar vagy a keresenkenti
  handoff ownership a gyanus.
- Ha a runtime log `compiled device mismatch` vagy `manual_split_contract_mismatch` sort ir, akkor a Stage-1 / Stage-2 tenyleges OpenVINO hozzarendeles eltert a vart `GPU.1` / `GPU.0` parositasrol, vagy a manifest hidden-state szerzodese nem egyezik a compiled porttal.
- Ha a szerver `split_memory_budget_error` kodot ad vissza, akkor a keres a validalt context budgetet vagy az egyik
  stage memoria-budgetjet lephette tul. A jelenlegi US2 strategiaban **mindket stage sajat KV-cache budgettel szamol**.
- Ilyenkor csokkentsd a prompt meretet / `max_tokens` erteket, vagy novelj kulon `split_stage_1_memory_budget_gb`
  es `split_stage_2_memory_budget_gb` limitet a profilon.
- A manual split runtime log kulcsszavaihoz hasznald a kovetkezo szurest:

```powershell
Select-String -Path build\logs\masodik-runtime-manual-split.log -Pattern "split_stage|split|handoff"
```

Long-context smoke:

```powershell
.\tests\smoke\test_manual_split_long_context.ps1 -BaseUrl http://127.0.0.1:29011
.\tests\smoke\test_manual_split_compiled_128k.ps1 -BaseUrl http://127.0.0.1:29011
```
## Manual split stage diagnosztika

### Health payload ellenorzese

A manual split health payloadban a kovetkezo mezoknek kell letezniuk `split_mode=true` eseten:

| Mezo | Ervenyes ertekek | Jelentes |
|------|-----------------|----------|
| `stage_1_state` | `uninitialized`, `starting`, `ready`, `failed` | A dGPU (stage-1) allapota |
| `stage_2_state` | `uninitialized`, `starting`, `ready`, `failed` | Az iGPU (stage-2) allapota |
| `handoff_state` | `idle`, `ready`, `consumed`, `failed` | A stage-1 → stage-2 atadas allapota |
| `last_failure_scope` | `startup`, `stage-1`, `stage-2`, `handoff`, ures | Melyik retegben tortent a hiba |
| `last_failure_summary` | strukturalt string | Egysoros osszefoglalas a hiba okairol |
| `request_path_mode` | `placeholder`, `compiled`, `degraded` | A keres tenylegesen compiled ketstage-es uton futott-e |

### Diagnosztikai parancsok

```powershell
# Health payload vizsgalata
(Invoke-RestMethod http://127.0.0.1:29011/health) | Select-Object split_mode, stage_1_state, stage_2_state, handoff_state, request_path_mode, last_failure_scope, last_failure_summary

# Split stage log sorok szurese
Select-String -Path build\logs\masodik-runtime.log -Pattern "split_stage|split_failure_summary|manual_split_runtime_failure"

# Diagnostics smoke validacio
.\tests\smoke\test_manual_split_diagnostics.ps1 -BaseUrl http://127.0.0.1:29011

# Compiled 128k smoke validacio
.\tests\smoke\test_manual_split_compiled_128k.ps1 -BaseUrl http://127.0.0.1:29011
```

### Tipikus hibamintak

1. **Stage-2 (iGPU) hiba**: `stage_1_state=ready`, `stage_2_state=failed`, `last_failure_scope=stage-2`
   - Ok: az iGPU VRAM nem elegendo, vagy a shared memory korlat
   - Teendo: csokkentsd a `split_stage_2_memory_budget_gb` erteket

2. **Stage-1 (dGPU) hiba**: `stage_1_state=failed`, `last_failure_scope=stage-1`
   - Ok: a B580 12GB VRAM nem elegendo a stage-1 retegekhez
   - Teendo: ellenorizd az artifact meretet es a `stage_1_memory_budget_gb` erteket

3. **Handoff hiba**: `handoff_state=failed`, `last_failure_scope=handoff`
    - Ok: a hidden-state atadasnal adatserules vagy timeout
    - Teendo: ellenorizd a handoff szerzodest a manifestben, a `manual_split_handoff_buffer_ready` logsort, es hogy a Stage-2 tenyleg a `hidden_states_from_stage_1` portot kapja-e.

4. **Fallback block**: `fallback_detected=true`, `last_failure_scope=startup`
   - Ok: azonos GPU device vagy CPU device lett konfigurálva
   - Teendo: a split profil ket kulon GPU-t kell hasznaljon

## Manual split long-context guardok

- A `split_memory_budget_error` azt jelzi, hogy a keres becsult token- vagy KV-cache igenye kilog a stage-enkenti budgetbol. Ilyenkor a pipeline nem lesz unhealthy, de a keres szabalyosan 503-mal elutasithato.
- A `split_overload_error` low-concurrency split profilnal azt jelzi, hogy az aktualis queue policy (`reject_new`) nem enged tovabbi kerest a futo request melle.
- Sikeres split long-context keres utan a `/health` valaszban a `memory_summary.stage_1` es `memory_summary.stage_2` mezokben `required:` es `budget:` jelzesnek kell latszania, a `memory_summary.system` mezoben pedig `prompt_tokens=` es `total_tokens=` varhato.
- Ha a split runtime a valodi artifacttal sem tud detokenizalt szoveget adni, es tovabbra is `Compiled split infer sikeres...` tartalom erkezik, az tipikusan azt jelzi, hogy a split tokenizer/detokenizer par nem toltodott be. Ilyenkor keresd a `manual_split_tokenizer_ready` vagy `manual_split_tokenizer_missing` logsort.
- A runtime logban keresd ezeket a strukturalt sorokat:
  - `manual_split_request_begin`
  - `manual_split_guard_reject`
  - `manual_split_stage_transition`
  - `manual_split_handoff_transfer`
  - `manual_split_handoff_buffer_ready`
  - `manual_split_decode_step`
  - `manual_split_request_complete`

## Long-context uzemeltetesi ajanlas

- A `b580-manual-split` profil szandekosan low-concurrency uzemre van allitva: `max_concurrent_requests=1`, `overflow_behavior=reject_new`.
- Ez direkt ved a RAM-felfutas ellen: a masodik, varatlan keres inkabb azonnali overload hibaval alljon meg, mint hogy ujabb nagy contextet probaljon elokesziteni.
- Ha tartosan `split_memory_budget_error` jon, eloszor a contextmeretet csokkentsd; csak utana emelj stage-budgetet.
