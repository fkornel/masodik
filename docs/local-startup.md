# Helyi startup jegyzetek

## Cel

Ez a leiras a helyi `masodik-launcher` explicit ket-stage-es manual split inditasat rogziti a cut23-tail topologiara (GPU.1 dGPU stage-1, GPU.0 iGPU stage-2).

## Port konvencio

- **Aktiv vegpont**: `http://127.0.0.1:29015` — a `b580-manual-split-cut23-tail-32k-u8.json` profil portja

## Launcher hasznalat

```powershell
.\build\src\main\masodik-launcher.exe `
  --profile config\profiles\b580-manual-split-cut23-tail-32k-u8.json `
  --model-root "C:\dev\copilot\masodik\build\generated-manual-split-cut23-tail-32k-u8"
```

Gyors startup-validaciohoz:

```powershell
.\build\src\main\masodik-launcher.exe --validate-and-stop
.\tools\scripts\validate-quickstart.ps1
```

## Mit csinal a launcher

1. betolti a cut23-tail split profilt es a split artifact roott
2. runtime dependency auditot keszit
3. stage-1-et a B580 dGPU-n (GPU.1), stage-2-t az iGPU-n (GPU.0) kompilal
4. in-process HTTP szervert indit a 29015-os porton
5. kiteszi a health, models es chat endpointokat (stream es non-stream, tool-calls)
6. a `request_path_mode=compiled` allapotban valodi ket-GPU-s infert hajt vegre

## Aktiv endpointok

- `GET /health`
- `GET /v1/models`
- `POST /v1/chat/completions`

### Streaming hasznalat

```powershell
.\tests\smoke\test_chat_completion_stream.ps1 `
  -BaseUrl http://127.0.0.1:29015
```

### Tool calling hasznalat

```powershell
.\tests\smoke\test_tool_calling.ps1 `
  -BaseUrl http://127.0.0.1:29015

.\tests\smoke\test_tool_calling_stream.ps1 `
  -BaseUrl http://127.0.0.1:29015
```

### Request audit es diagnozis

A szerver minden chat kerest strukturalt `request_audit {json}` sorral logol:

```text
build\logs\masodik-runtime-manual-split-cut23-tail-32k-u8.log
```

```powershell
Select-String -Path build\logs\masodik-runtime-manual-split-cut23-tail-32k-u8.log -Pattern "request_audit"
.\tests\smoke\test_request_audit_logging.ps1 -BaseUrl http://127.0.0.1:29015
```

## Smoke tesztek

```powershell
Invoke-WebRequest http://127.0.0.1:29015/health
Invoke-WebRequest http://127.0.0.1:29015/v1/models
Invoke-WebRequest http://127.0.0.1:29015/v1/chat/completions -Method Post -ContentType "application/json" -Body '{"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia"}]}'
```

Split profilra es valodi artifactra:

```powershell
.\tests\smoke\test_profile_matrix.ps1 -ManualSplitModelRoot "build\generated-manual-split-cut23-tail-32k-u8"
```

## Manual split startup

### Generated split artifact eloallitasa (cut23-tail)

```powershell
.\tools\scripts\generate-manual-split-artifact.ps1 `
  -OutputRoot .\build\generated-manual-split-cut23-tail-32k-u8 `
  -Profile config\profiles\b580-manual-split-cut23-tail-32k-u8.json `
  -MaxContextTokens 32768 `
  -ValidatedContextTokens 32768
```

A kelet artifact felepitese:

```text
build\generated-manual-split-cut23-tail-32k-u8\
  split-manifest.json         cut_after_layer = layers.23 -> tail
  stage-1\openvino_model.xml  transformer blokkok 0-23 (B580 dGPU)
  stage-2\openvino_model.xml  tail (iGPU)
  tokenizer\tokenizer.json
```

### Gepre szabott tervezesi budget (B580 + Core Ultra 7 265K, cut23-tail)

| Terulet | Mert meret | Megjegyzes |
|---------|------------|------------|
| dGPU dedicated max | ~11.89 GB | Stage-1 sulyok + KV-cache a B580-on |
| iGPU shared/local max | ~1.87 GB | Stage-2 tail, minimalis abszolut meret |
| Peak working set | ~14.81 GB | Teljes folyamat max memoria |
| Peak private bytes | ~19.23 GB | Teljes privat kepterfoglal |

A tail split nyero topologia azert mukodik, mert a stateful attention teljes egeszeben a dGPU-n marad;
az iGPU csak a lezaro fejeket (normalization + lm_head) vigyen.

### Inditas valodi artifacttal

```powershell
.\build\src\main\masodik-launcher.exe `
  --profile config\profiles\b580-manual-split-cut23-tail-32k-u8.json `
  --model-root "C:\dev\copilot\masodik\build\generated-manual-split-cut23-tail-32k-u8"
```

Elvarasok:

1. a `model-root` tartalmazza a `split-manifest.json` fajlt
2. a manifest `validated_for_profile` mezoje `b580-manual-split-cut23-tail-32k-u8`
3. a manifest `cut_after_layer = layers.23 -> tail`
4. a startup `stage_1_state=ready`, `stage_2_state=ready`, `fallback_detected=false`
5. a startup nem eshet vissza sem CPU-ra, sem egyetlen GPU-ra

Manual split health ellenorzes:

```powershell
Invoke-WebRequest http://127.0.0.1:29015/health
```

Vart health payload mezok:

- `split_mode = true`
- `split_profile = b580-manual-split-cut23-tail-32k-u8`
- `stage_1_state = ready`
- `stage_2_state = ready`
- `handoff_state = consumed` (sikeres keres utan)
- `last_failure_scope` (hiba eseten)
- `fallback_detected = false`
- `memory_summary`

Valos quickstart validaciohoz:

```powershell
$env:MASODIK_MANUAL_SPLIT_MODEL_ROOT = "C:\dev\copilot\masodik\build\generated-manual-split-cut23-tail-32k-u8"
.\tools\scripts\validate-quickstart.ps1 -ManualSplitModelRoot $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT
```

Ez a kor csak akkor tekintheto PASS-nak, ha:

1. a startup `split_mode=true`, `fallback_detected=false`, `stage_1_state=ready`, `stage_2_state=ready`
2. a 32k smoke nem kap `Placeholder valasz` tartalmat
3. a diagnostics smoke latja a teljes split health payloadot
4. a proof measurement `request_succeeded=true`