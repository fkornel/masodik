# Data Model: NPU Embedding Serving

**Feature**: 003-npu-embedding-serving
**Date**: 2026-04-07

## Entities

### EmbeddingRequest

A kliens altal kuldott embedding keres. OpenAI `/v1/embeddings` kompatibilis.

| Mezo | Tipus | Kotelezo | Leiras |
|------|-------|----------|--------|
| model | string | igen | Az embedding modell azonositoja (pl. "Qwen3-Embedding-0.6B-int4-cw-ov") |
| input | string \| string[] | igen | Egyetlen szoveg vagy szovegtomb |
| encoding_format | string | nem | Jelenleg csak "float" tamogatott (alapertelmezett) |

### EmbeddingResponse

A szerver valasza embedding vektorokkal.

| Mezo | Tipus | Leiras |
|------|-------|--------|
| object | string | Mindig "list" |
| data | EmbeddingObject[] | Embedding eredmenyek tombje |
| model | string | A hasznalt modell azonositoja |
| usage | EmbeddingUsage | Token felhasznalasi statisztikak |

### EmbeddingObject

Egyetlen szoveg embedding eredmenye.

| Mezo | Tipus | Leiras |
|------|-------|--------|
| object | string | Mindig "embedding" |
| embedding | float[] | A normalizalt embedding vektor (1024 dimenzio) |
| index | int | A bemeneti szoveg indexe (0-alapu) |

### EmbeddingUsage

Token felhasznalasi statisztikak.

| Mezo | Tipus | Leiras |
|------|-------|--------|
| prompt_tokens | int | Az input szovegekben levo tokenek szama |
| total_tokens | int | Osszes felhasznalt token (embedding-nel megegyezik a prompt_tokens-szel) |

### EmbeddingPipelineState

Az embedding szolgaltatas belso allapota.

| Mezo | Tipus | Leiras |
|------|-------|--------|
| state | enum | uninitialized / loading / ready / degraded / failed |
| model_name | string | A betoltott modell neve |
| device | string | Az NPU eszkoz azonosito ("NPU") |
| embedding_dimension | int | A kimeneti vektor merete (1024) |
| error_message | string | Utolso hiba leirasa (ha van) |

### EmbeddingProfileConfig

A profil JSON embedding szekcioja.

| Mezo | Tipus | Alapertelmezett | Leiras |
|------|-------|-----------------|--------|
| enabled | bool | false | Embedding szolgaltatas engedelyezese |
| model_name | string | - | Modell azonosito |
| model_path | string | - | Abszolut utvonal a modell konyvtarhoz |
| device | string | "NPU" | OpenVINO eszkoz string |
| reject_cpu_fallback | bool | true | CPU fallback tiltasa |
| endpoint_path | string | "/v1/embeddings" | HTTP endpoint utvonal |

## Relationships

```
EmbeddingRequest ---[1:N]---> EmbeddingObject (input szovegenkent egy embedding)
EmbeddingResponse ---[1:1]---> EmbeddingUsage
EmbeddingPipelineState ---[1:1]---> HealthResponse (bovites)
EmbeddingProfileConfig ---[1:1]---> ServingProfile (bovites)
```

## State Transitions

### EmbeddingPipelineState

```
uninitialized → loading → ready        (sikeres NPU betoltes)
uninitialized → loading → failed        (NPU nem elerheto / modell hiba)
ready → degraded                        (runtime hiba)
degraded → ready                        (sikeres ujratoltodes)
```

Fontos: Az embedding allapot fuggetlen a chat pipeline allapottol. Az embedding "failed" allapot nem befolyasolja a chat "ready" allapotot.
