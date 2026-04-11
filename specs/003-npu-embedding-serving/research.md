# Research: NPU Embedding Serving

**Feature**: 003-npu-embedding-serving
**Date**: 2026-04-07

## 1. OpenVINO GenAI TextEmbeddingPipeline

**Decision**: Az `ov::genai::TextEmbeddingPipeline` osztaly hasznalata az embedding inferencia megvalositasahoz.

**Rationale**: Az SDK tartalmaz dedikalt embedding pipeline-t (`openvino/genai/rag/text_embedding_pipeline.hpp`), amely kezeli a tokenizalast, a batch inferencia-t, a pooling-ot es a normalizalast. Nem szukseges sajat low-level `ov::Core` + `ov::InferRequest` implementacio.

**Alternatives considered**:
- Kozvetlen `ov::Core::compile_model()` + `ov::InferRequest`: Nagyobb kontroll, de a tokenizalast, pooling-ot es normalizalast kezi implementacio igenyel. Nincs elonye, mert a TextEmbeddingPipeline pont ezt vegzi.
- Kulso embedding service (pl. ollama): A projekt celja helyi, nativ C++ serving. Kulso dependency nem felel meg az architekturanak.

## 2. NPU eszkoz tamogatas

**Decision**: Az `"NPU"` device string hasznalata az OpenVINO-ban az Intel AI Boost (NPU) eszkozhoz.

**Rationale**: Az OpenVINO SDK a C API peldakban `"NPU"` stringet hasznal. Az Intel Core 7 Ultra 265k processzor integralt NPU-ja tamogatott. A Qwen3-Embedding-0.6B-int4 modell meretenel (0.6B parameter, int4) az NPU kapacitasa boven elegendo.

**Alternatives considered**:
- CPU: Mukodne, de lassabb es feleslegesen terheli a CPU-t, ami mar a rendszer tobbi feladatara kell.
- GPU: Mindket GPU (dGPU + iGPU) mar foglalt a chat split serving altal. Nem lenne eszkoz-konfliktus-mentes.

## 3. Qwen3-Embedding-0.6B modell jellemzok

**Decision**: A `config.json` alapjan a modell 1024 dimenzios embedding-et ad. Az architektura `Qwen3ForCausalLM`, de embedding celra hasznalva (pooling reget).

**Rationale**: A `hidden_size: 1024` a modell embedding kimeneti meretkent funkcionial. A `TextEmbeddingPipeline` CLS vagy MEAN pooling-gal kepes a token-szintu kimeneteket egyetlen vektorra redukalni.

**Alternatives considered**:
- A modell nativan Qwen3ForCausalLM architektura (nem dedikalt SentenceTransformer). A TextEmbeddingPipeline Config-ban meg kell vizsgalni, hogy a MEAN pooling a legmegfelelobb-e, vagy CLS. A Qwen3-Embedding sorozat MEAN pooling-ot javasol.

## 4. Pooling strategia

**Decision**: MEAN pooling hasznalata, normalizalas bekapcsolva.

**Rationale**: A Qwen3-Embedding modellcsalad dokumentacioja MEAN pooling-ot javasol. A normalizalas biztositja, hogy a cosine similarity kozvetlenul osszehasonlithato legyen.

**Alternatives considered**:
- CLS pooling: Egyes modellek (BERT) ezt preferaljak, de a Qwen3-Embedding nem.
- LAST_TOKEN pooling: Autoregressive modelleknel hasznos, de embedding modellnel nem standard.

## 5. CPU fallback kezeles

**Decision**: Explicit NPU device koveteles, CPU fallback visszautasitasa.

**Rationale**: A meglevo architektura (`reject_cpu_fallback: true`) mintajat kovetjuk. Ha az NPU nem erheto el, inkabb egyertelmu hiba, mint csendes teljesitmenyromloas.

**Alternatives considered**:
- Silent CPU fallback: Mukodne, de a felhasznalo nem tudna, hogy nem az NPU-t hasznalja. Ez ellentetes a constitution II. elvevel (hardware-tuned serving).
