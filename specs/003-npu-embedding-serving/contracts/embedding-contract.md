# Embedding API Contract

**Feature**: 003-npu-embedding-serving
**Date**: 2026-04-07
**Kompatibilitas**: OpenAI `/v1/embeddings` API

## Endpoint

```
POST /v1/embeddings
Content-Type: application/json
```

## Request

```json
{
  "model": "Qwen3-Embedding-0.6B-int4-cw-ov",
  "input": "A szoveg amit embedding-elni szeretnenk"
}
```

Batch variáns:

```json
{
  "model": "Qwen3-Embedding-0.6B-int4-cw-ov",
  "input": [
    "Elso szoveg",
    "Masodik szoveg",
    "Harmadik szoveg"
  ]
}
```

Opcionalis mezo:
- `encoding_format`: csak `"float"` tamogatott (alapertelmezett)

## Response (200 OK)

```json
{
  "object": "list",
  "data": [
    {
      "object": "embedding",
      "embedding": [0.0123, -0.0456, 0.0789, ...],
      "index": 0
    }
  ],
  "model": "Qwen3-Embedding-0.6B-int4-cw-ov",
  "usage": {
    "prompt_tokens": 12,
    "total_tokens": 12
  }
}
```

Az `embedding` tomb merete mindig 1024 (a modell hidden_size-a).

## Hibakodok

| HTTP status | Eset | Body pelda |
|-------------|------|------------|
| 400 | Ures input, ervenytelen formatum | `{"error":{"message":"Az input mezo nem lehet ures.","type":"invalid_request_error","code":"bad_request"}}` |
| 404 | Ismeretlen modell | `{"error":{"message":"A kert modell nem elerheto.","type":"model_not_found","code":"not_found"}}` |
| 503 | Embedding pipeline nem kesz | `{"error":{"message":"Az embedding szolgaltatas nem elerheto.","type":"service_unavailable","code":"service_unavailable"}}` |

## Health bovites

A `/health` valasz uj mezoi:

```json
{
  "ready": true,
  "embedding_state": "ready",
  "embedding_model": "Qwen3-Embedding-0.6B-int4-cw-ov",
  "embedding_device": "NPU",
  ...
}
```

## Models bovites

A `/v1/models` valasz uj bejegyzese:

```json
{
  "data": [
    {
      "id": "gpt-oss-20b-int4-ov",
      "object": "model",
      "owned_by": "local"
    },
    {
      "id": "Qwen3-Embedding-0.6B-int4-cw-ov",
      "object": "model",
      "owned_by": "local",
      "type": "embedding"
    }
  ]
}
```

## Teszt peldak

### Egyedi embed

```bash
curl -s http://localhost:29011/v1/embeddings \
  -H "Content-Type: application/json" \
  -d '{"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":"Hello world"}'
```

Elvart: 200, `data[0].embedding` hossza 1024, `usage.prompt_tokens > 0`.

### Batch embed

```bash
curl -s http://localhost:29011/v1/embeddings \
  -H "Content-Type: application/json" \
  -d '{"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":["foo","bar","baz"]}'
```

Elvart: 200, `data` tomb merete 3, minden `index` kulon (0, 1, 2).
