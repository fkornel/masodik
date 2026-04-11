# Quickstart: NPU Embedding Serving

**Feature**: 003-npu-embedding-serving
**Date**: 2026-04-07

## Elofeltetelek

1. Intel Core 7 Ultra 265k processzor NPU-val
2. OpenVINO GenAI SDK telepitve (mar a projektben megvan)
3. Qwen3-Embedding-0.6B-int4-cw-ov modell letoltve az AI Playground-dal

## 1. Profil konfiguralas

A `config/profiles/b580-manual-split-cut23-tail-32k-u8.json` fajlba adjunk hozza embedding szekciot:

```json
{
  "embedding": {
    "enabled": true,
    "model_name": "Qwen3-Embedding-0.6B-int4-cw-ov",
    "model_path": "C:\\Users\\jezus\\AppData\\Local\\Programs\\AI Playground\\resources\\models\\LLM\\embedding\\openVINO\\OpenVINO---Qwen3-Embedding-0.6B-int4-cw-ov",
    "device": "NPU",
    "reject_cpu_fallback": true,
    "endpoint_path": "/v1/embeddings"
  }
}
```

## 2. Szerver inditas

```powershell
.\build\src\main\masodik-launcher.exe `
  --profile config\profiles\b580-manual-split-cut23-tail-32k-u8.json `
  --model-root build\generated-manual-split-cut23-tail-32k-u8
```

A szerver betolti a chat modellt (GPU split) es az embedding modellt (NPU) parhuzamosan.

## 3. Health ellenorzes

```bash
curl -s http://localhost:29011/health | jq '.embedding_state, .embedding_model, .embedding_device'
```

Elvart kimenet:
```
"ready"
"Qwen3-Embedding-0.6B-int4-cw-ov"
"NPU"
```

## 4. Embedding keres

### Egyedi szoveg

```bash
curl -s http://localhost:29011/v1/embeddings \
  -H "Content-Type: application/json" \
  -d '{"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":"Mi a fovaros Magyarorszagon?"}'
```

### Batch keres

```bash
curl -s http://localhost:29011/v1/embeddings \
  -H "Content-Type: application/json" \
  -d '{"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":["Budapest","Pecs","Debrecen"]}'
```

## 5. Modell lista

```bash
curl -s http://localhost:29011/v1/models
```

A valaszban ket modell latszik: a chat modell es az embedding modell.

## 6. Smoke teszt

```powershell
.\tests\smoke\test_embedding_endpoint.ps1
```

## Hibakereses

Ha az embedding "failed" allapotban van:

1. Ellenorizd a log-ot: `build/logs/masodik-runtime-*.log`
2. Ellenorizd, hogy az NPU driver telepitve van-e: `openvino_available_devices`
3. Ellenorizd a modell utvonalat a profilban
4. Ha az NPU nem elerheto, a hiba egyertelmu lesz a health-ben (nem csendes CPU fallback)
