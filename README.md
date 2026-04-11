# masodik

Helyi, OpenAI-kompatibilis C++23 szolgaltatas OpenVINO GenAI alapon, a `gpt-oss-20b-int4-ov` modellhez hangolva.

## Fókusz

- **Explicit kétstage-es manual split**: stage-1 a B580 dGPU-n (GPU.1), stage-2 az iGPU-n (GPU.0)
- `cut23-tail` topologia: a transformer blokkok (0-23) a dGPU-n, a tail (lezaro fejek) az iGPU-n futnak
- 32k kontextus + INT8 (u8) KV-cache, `reject_cpu_fallback=true`
- OpenAI-kompatibilis stream/non-stream chat completion es tool_calls tamogatas
- magyar diagnosztika és üzemeltetési megjegyzések

## Egyetlen aktív profil

**`config\profiles\b580-manual-split-cut23-tail-32k-u8.json`**

| Mező | Érték |
|---|---|
| stage_1_device | GPU.1 (B580 dGPU) |
| stage_2_device | GPU.0 (Intel iGPU) |
| desired_context_tokens | 32768 |
| kv_cache_precision | u8 |
| bind_port | 29015 |
| reject_cpu_fallback | true |

## Gyors indulás

```powershell
.\tools\scripts\configure-local.ps1
.\tools\scripts\build-and-test.ps1
.\tools\scripts\validate-quickstart.ps1
```

Valódi split artifact-tal (pl. generált `build\generated-manual-split-cut23-tail-32k-u8\`):

```powershell
$env:MASODIK_MANUAL_SPLIT_MODEL_ROOT = "C:\dev\copilot\masodik\build\generated-manual-split-cut23-tail-32k-u8"
.\tools\scripts\validate-quickstart.ps1 -ManualSplitModelRoot $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT
```

Közvetlen indítás:

```powershell
.\build\src\main\masodik-launcher.exe `
  --profile config\profiles\b580-manual-split-cut23-tail-32k-u8.json `
  --model-root build\generated-manual-split-cut23-tail-32k-u8
```

## Split artifact generálás

A split artifactot a `masodik-split-artifact-tool` CLI eszköz állítja elő:

```powershell
.\tools\scripts\generate-manual-split-artifact.ps1 `
  -SourceModelRoot "C:\...\gpt-oss-20b-int4-ov" `
  -OutputDir "build\generated-manual-split-cut23-tail-32k-u8" `
  -Profile "config\profiles\b580-manual-split-cut23-tail-32k-u8.json"
```

A kész artifact felépítése:

```text
build\generated-manual-split-cut23-tail-32k-u8\
├── split-manifest.json   # cut_after_layer = layers.23 -> tail
├── stage-1\              # transformer blokkok 0-23 (B580 dGPU)
├── stage-2\              # tail (iGPU)
└── tokenizer\
```

## Proof allapot

A Phase 9 proof mérés sikeres volt:

| Metrika | Érték |
|---|---|
| request_succeeded | true |
| request_path_mode | compiled |
| stage_1_execution_devices | GPU.1 |
| stage_2_execution_devices | GPU.0 |
| handoff_state | consumed |
| peak working set | ~14.81 GB |
| peak private bytes | ~19.23 GB |
| dGPU dedicated max | ~11.89 GB |
| iGPU shared/local max | ~1.87 GB |

Proof artifact: `build\artifacts\long-context-measure\cut23-tail-u8-real-32k-measurement.json`

## Benchmark és validáció

- smoke suite: `.\tools\scripts\run-smoke-suite.ps1`
- split profile matrix: `.\tests\smoke\test_profile_matrix.ps1 -ManualSplitModelRoot <root>`
- split proof smoke: `cmake --build build --target masodik-manual-split-proof-smoke`
- benchmark export: `.\tools\profiling\export-benchmark-results.ps1`

## Tool calling használat

```powershell
$body = @'
{
  "model": "gpt-oss-20b-int4-ov",
  "messages": [
    { "role": "user", "content": "Sorold fel a nyitott PR-eket a megfelelo toollal." }
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "exec",
        "description": "Lokalis shell parancs futtatasa a kliens oldalan.",
        "parameters": {
          "type": "object",
          "properties": {
            "command": { "type": "string" }
          },
          "required": ["command"]
        }
      }
    }
  ],
  "tool_choice": "auto",
  "stream": false
}
'@

Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:29015/v1/chat/completions `
  -ContentType "application/json" `
  -Body $body
```

## Request audit

A szerver minden chat kérést strukturált `request_audit {json}` sorral logol a
`build\logs\masodik-runtime-manual-split-cut23-tail-32k-u8.log` fájlba.


```powershell
.\tests\smoke\test_request_audit_logging.ps1 -BaseUrl http://127.0.0.1:29000
```

## Megjegyzés

A `stream: true` kereseknel a szolgaltatas `text/event-stream` valasszal dolgozik; a hagyomanyos `stream: false`
keresek tovabbra is JSON `chat.completion` payloadot adnak. Tool-calling esetben a szerver csak strukturalt
`tool_calls` payloadot epit, a tenyleges tool futtatasa a kliensoldali orchestrator feladata.

A tiszta GPU profil celja nem garancia, hanem meres: megmutatja, hogy a modell 32768 context mellett egyaltalan betoltheto-e teljesen VRAM-ba, es ha igen, mennyi a hybrid felar.
