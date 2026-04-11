# Performance Baseline Guide

## Cél

Ez a jegyzet a helyi `b580-manual-split-cut23-tail-32k-u8` split serving benchmarkokat és a megmért baseline-okat rögzíti.

## Cut23-tail proof baseline (Phase 9 mérés)

A Phase 9 sikeres `cut23-tail + u8 + 32k` mérése:

| Metrika | Mért érték |
|---|---|
| request_succeeded | true |
| request_path_mode | compiled |
| stage_1_execution_devices | GPU.1 |
| stage_2_execution_devices | GPU.0 |
| handoff_state | consumed |
| peak working set | ~14.81 GB |
| peak private bytes | ~19.23 GB |
| dGPU dedicated max | ~11.89 GB |
| dGPU local max | ~7.05 GB |
| iGPU shared/local max | ~1.87 GB |

Proof artifact: `build\artifacts\long-context-measure\cut23-tail-u8-real-32k-measurement.json`

## Benchmarkok

- `benchmark_cold_start`: teljes modellbetöltési indulási idő (non-split baseline)
- `benchmark_first_response`: első válasz késleltetése
- `benchmark_restart_stability`: két egymás utáni hideg indulás összesített ideje
- `benchmark_parallel_requests`: korlátozott párhuzamos kéréskiszolgálás
- `benchmark_long_queue`: hosszú sor melletti health és queue láthatóság
- `benchmark_streaming_mixed_load`: streamelt és nem streamelt kérések vegyes terhelése
- `benchmark_tool_call_first_delta`: az elso streamelt tool-call delta kesleltetese
- `benchmark_32768_context`: 32768 context cut23-tail split profil alatti stabilitás
- `benchmark_compare_gpu_vs_hybrid_32768`: cut23-tail 32k kontextus teljesítménymérés

## Manual split baseline benchmarkok

- `benchmark_manual_split_cold_start`: a ket kulon stage IR startup + warmup ideje
- `benchmark_manual_split_long_context`: split profil alatti hosszabb contextes valaszido es `handoff_state=consumed`
- `benchmark_manual_split_handoff_latency`: stage-1 -> stage-2 atadas korrelacios baseline
- `benchmark_manual_split_diagnostics_overhead`: split-aware `/health` payload overhead
- `benchmark_manual_split_stability_proof`: tobbszori compiled split keres utani proof-metadata jelenlet

### Manual split memoria-cel allapot (cut23-tail)

| Terület | Mért érték | Megjegyzés |
|---------|------------|------------|
| dGPU dedicated | ~11.89 GB | Transformer blokkok 0-23 + KV-cache |
| iGPU shared/local | ~1.87 GB | Tail (normalization + lm_head) |
| Working set csúcs | ~14.81 GB | Teljes folyamat csúcs |
| Private bytes csúcs | ~19.23 GB | Privat memória csúcs |

### Manual split elfogadasi minimum

- `manual_split_long_context_status=200`
- `manual_split_long_context_handoff_consumed=true`
- `manual_split_handoff_last_state=consumed`
- `manual_split_diagnostics_all_split_fields=true`
- `manual_split_stability_proof_compiled=true`
- `manual_split_stability_proof_stage_1_execution_devices=present`
- `manual_split_stability_proof_stage_2_execution_devices=present`
- a response body **nem** tartalmazhat `Placeholder valasz` szoveget

### Export hasznalat

```powershell
$env:MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT = "C:\dev\copilot\masodik\build\generated-manual-split-cut23-tail-32k-u8"
.\tools\profiling\export-benchmark-results.ps1
```

### Hardveres proof kor

```powershell
$env:MASODIK_MANUAL_SPLIT_MODEL_ROOT = "C:\dev\copilot\masodik\build\generated-manual-split-cut23-tail-32k-u8"
.\tests\smoke\test_manual_split_hardware_proof.ps1
```

A proof akkor hiteles, ha a keletkezo JSON artifact egyszerre rogzit:

- `request_path_mode = compiled`
- `stage_1_execution_devices = GPU.1`
- `stage_2_execution_devices = GPU.0`
- `handoff_state = consumed`
- legalabb ket aktiv adapter-LUID mintat a launcher process alatt

## Értelmezés

- A `mixed_load_health` akkor jó, ha a futás közben az `active_streams`, a `peak_active_streams` és a streamhiba mezők látszanak.
- A `context_32768_status=200` önmagában stabilitási minimum; a `handoff_state=consumed` az igazi proof.
- A `reject_cpu_fallback=true` azt jelenti, hogy bármilyen CPU-ra esés FAIL-ként mutatkozik; nincs csendes degradálás.

## Kompatibilitasi baseline

- A non-stream tool-calling valasz akkor kompatibilis, ha a kliens `content = null`, `tool_calls[]` es
  `finish_reason = "tool_calls"` triot kapja.
- A streaming tool-calling valasz akkor kompatibilis, ha a kliens legalabb egy `choices[].delta.tool_calls[]`
  SSE payloadot kap, az arguments reszek osszeallnak, es a stream `data: [DONE]` lezarassal zarul.

## Request audit overhead baseline

- A request audit jelenlegi szerzodese szerint minden chat keres legfeljebb egy strukturalt `request_audit`
  logsorral novelheti a runtime logot.
- A `benchmark_parallel_requests` futas most kiirja a `parallel_requests_expected_audit_records` metrikat is,
  hogy a meresi logban egyszeruen osszevetheto legyen a keresek szama es a logba kerult audit sorok mennyisege.
- A baseline ertekelese soran nem az a cel, hogy az audit logging nulla koltsegu legyen, hanem hogy ne fedje el
  a `parallel_requests_total_ms`, `stream_first_token_ms` vagy `tool_call_first_delta_ms` trendjeit.
- Ha a request audit aktiv, a logmeretet a roviditett preview miatt elsosorban a keresszam es nem a teljes
  promptmeret novelheti.
