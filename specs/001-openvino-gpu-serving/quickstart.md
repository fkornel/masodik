# Quickstart: Helyi hibrid AI kiszolgalas

## Cel

Ez a quickstart a tervezett in-process vegallapot validalasi utjat rogziti a jelenlegi celgepen.

## 1. Elofeltetelek ellenorzese

Ellenorizd, hogy a kovetkezo elemek elerhetok:

```powershell
Test-Path "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe"
Test-Path "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
Test-Path "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO\.venv\Lib\site-packages\openvino\cmake"
Test-Path "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO\ovms\openvino_genai.dll"
Test-Path "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\models\LLM\openvino\OpenVINO---gpt-oss-20b-int4-ov\openvino_model.xml"
```

## 2. Build es teszt

```powershell
.\tools\scripts\configure-local.ps1
.\tools\scripts\build-and-test.ps1
.\tools\scripts\validate-quickstart.ps1
```

## 3. Lokalis konfiguracio

Az elvart lokalis konfiguracio legalabb a kovetkezoket tartalmazza:

- bind host
- bind port
- model root
- primary device
- hybrid mod be/ki
- cache directory
- queue policy
- timeout policy
- health/chat/models endpoint pathok

Javasolt konfiguracios forras:

```text
config/profiles/b580-cpu-hetero.json
```

## 4. Szolgaltatas inditasa

A tervezett launcher futtatasi forma:

```powershell
.\build\src\main\masodik-launcher.exe --profile config\profiles\b580-cpu-hetero.json
```

## 5. Health ellenorzes

```powershell
Invoke-WebRequest http://127.0.0.1:29011/health
```

### OpenClaw endpoint konvencio

- OpenClaw manualis probakhoz a rogzitett vegpont: `http://127.0.0.1:29011`
- smoke/regresszios korokhoz kulon tesztport is hasznalhato, peldaul `29000`

Igy az automata validacio nem keveredik az OpenClaw-bol erkezo valos keresorral.

### Request logging ellenorzes

Ha OpenClaw-bol nem egyertelmu, hogy a kliens tenylegesen kuldott-e `tools` vagy `tool_choice` mezot,
a kovetkezo kor request audit loggingja alapjan ezt utolag is ellenorizni kell tudni. Az elvart audit
kimenet legalabb ezeket a mezoket tartalmazza:

- `request_id`
- `stream`
- `tools_present`
- `tool_count`
- `tool_choice_kind`
- `tool_choice_name`
- `decision_path`
- `validation_outcome`

Az alapertelmezett runtime log:

```text
build\logs\masodik-runtime.log
```

Gyors request-audit regresszio:

```powershell
.\tests\smoke\test_request_audit_logging.ps1 `
  -BaseUrl http://127.0.0.1:29000 `
  -LogPath build\logs\masodik-runtime.log
```

OpenClaw diagnozishoz az `X-Request-Id` valaszfejlecet es a log megfelelo `request_audit {json}` sorat kell
osszeparositani. A legfontosabb olvasatok:

- `tools_present = true` es `decision_path = "content_only"`: a kliens kuldott tool listat, de a szerver nem
  ment tool-call agba
- `tool_choice_kind = "required"` es `decision_path = "tool_call_required"`: a kereskontextus tenylegesen
  kotelezett tool hivasra futott
- `tool_choice_kind = "named_function"` es `validation_outcome = "tool_schema_error"`: a kliens olyan named
  toolt kert, amely nincs jelen a `tools[]` listaban

## 6. Chat smoke teszt

```powershell
$body = @{
  model = "gpt-oss-20b-int4-ov"
  messages = @(
    @{ role = "user"; content = "Adj egy rovid valaszt." }
  )
} | ConvertTo-Json -Depth 5

Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:29000/v1/chat/completions `
  -ContentType "application/json" `
  -Body $body
```

### Streaming smoke teszt

```powershell
.\tests\smoke\test_chat_completion_stream.ps1 `
  -BaseUrl http://127.0.0.1:29000 `
  -BodyPath tests\fixtures\chat_request_stream.json
```

### Tool calling peldakeres

```powershell
$body = @'
{
  "model": "gpt-oss-20b-int4-ov",
  "messages": [
    { "role": "user", "content": "Sorold fel a nyitott PR-eket." }
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "exec",
        "description": "Lokalis parancs futtatasa a kliens oldalan.",
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
  -Uri http://127.0.0.1:29000/v1/chat/completions `
  -ContentType "application/json" `
  -Body $body
```

Elvart kimenet:

- `choices[0].finish_reason = "tool_calls"`
- `choices[0].message.content = null`
- `choices[0].message.tool_calls[0].function.name = "exec"`
- a `function.arguments` mezoben a kliens oldalon futtathato parancs szerepel

Smoke validacio:

```powershell
.\tests\smoke\test_tool_calling.ps1 `
  -BaseUrl http://127.0.0.1:29000 `
  -BodyPath tests\fixtures\chat_request_tool_call.json
```

### Streaming tool calling validacio

```powershell
.\tests\smoke\test_tool_calling_stream.ps1 `
  -BaseUrl http://127.0.0.1:29000 `
  -BodyPath tests\fixtures\chat_request_tool_call_stream.json
```

Elvart streaming viselkedes:

- a stream `text/event-stream` fejleccel indul
- a kezdo chunk tovabbra is `assistant` role deltaval kezd
- a tool-call payload `choices[].delta.tool_calls[]` SSE delta-kban epul fel
- a `function.arguments` tartalom tobb chunkbol is osszeallhat
- a terminalis chunk `finish_reason = "tool_calls"` ertekkel zar, majd `data: [DONE]` koveti
- a valaszfejlecben `X-Request-Id` jelenik meg, ami a runtime log `request_audit` sorahoz kotheto

## 7. Modellista ellenorzes

```powershell
Invoke-WebRequest http://127.0.0.1:29000/v1/models
```

## 8. Benchmark baseline

Az elso baseline futasnal legalabb ezeket rogzitjuk:

- cold start ido
- health keszreallas ideje
- elso valasz ideje
- elso streamelt valaszblokk ideje
- elso streamelt tool-call delta ideje
- 1x egymas utani keressorozat
- 5x egyideju keres
- 32768 context-cel inditott keres viselkedese
- queue viselkedes hosszu timeouttal
- pure GPU 32768 startup sikerul-e
- ha sikerul, mennyi a hybrid valaszido-felara a tiszta GPU-hoz kepest

Benchmark export:

```powershell
.\tools\profiling\export-benchmark-results.ps1
```

Kulon osszehasonlito scenario:

```powershell
.\build\benchmarks\masodik-benchmarks.exe benchmark_compare_gpu_vs_hybrid_32768
```

## 9. Hibaelvarasok

- Ha a modell nem toltheto be, a runtime nem maradhat csendben felkesz allapotban.
- Ha a hybrid profil nem tarthato, a runtime nem valthat csendes lassu degradacioba.
- Ha a queue megtelik vagy a policy megszegi a stabilitast, annak diagnosztizalhatonak kell lennie.
- Ha a modell tool hivas mellett dont, a `tool_calls` payload nem veszhet el sem non-stream, sem
  streaming uzemmodban.
- A szerver a tool definiciot csak strukturalt payloadga alakitja; a tool tenyleges futtatasa nem a
  backend, hanem a kliens felelossege.
