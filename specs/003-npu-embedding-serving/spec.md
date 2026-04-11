# Feature Specification: NPU Embedding Serving

**Feature Branch**: `003-npu-embedding-serving`  
**Created**: 2026-04-07  
**Status**: Complete  
**Input**: User description: "Embedding modell (Qwen3-Embedding-0.6B-int4-cw-ov) betoltese NPU-ra es OpenAI-kompatibilis /v1/embeddings endpoint kiszolgalasa. Intel Core 7 Ultra 265k processzor NPU-jat hasznaljuk."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Embedding vektor lekerese szoveghez (Priority: P1)

Fejlesztoként szeretnek egy helyi embedding endpointot, amelyre szoveg(eke)t kuldok es embedding vektorokat kapok vissza. Az endpoint az OpenAI `/v1/embeddings` szerzodesevel kompatibilis, igy a meglevo kliensek (pl. OpenClaw, LangChain, LlamaIndex) valtoztatas nelkul hasznalhatok.

**Why this priority**: Ez a feature alapfunkcioja. Embedding vektor nelkul sem keresesi, sem RAG pipeline nem epithet a helyi rendszerre.

**Independent Test**: Egy HTTP POST keres a `/v1/embeddings` endpointra egy rovid szoveggel. A valasz JSON-ben tartalmazza az embedding vektort a megfelelo dimensioszammal es az OpenAI sema szerint.

**Acceptance Scenarios**:

1. **Given** a szerver fut es a modell betoltodott, **When** a kliens POST-ol a `/v1/embeddings`-re egy `input` mezovel es `model` azonositoval, **Then** a valasz 200 OK, tartalmaz egy `data` tombot legalabb egy `embedding` objektummal, amelyben a `embedding` mezo float tomb a modell dimenzionaltasaval
2. **Given** a szerver fut, **When** a kliens tobb szoveget kuld egy keresben (tomb input), **Then** a valasz minden szoveghez kulon embedding objektumot ad vissza, megorzott sorrendben
3. **Given** a szerver fut, **When** a kliens ismeretlen modellt ker, **Then** a valasz 404 hibakoddal jelzi, hogy az adott modell nem elerheto

---

### User Story 2 - NPU-alapu modellbetoltes es health riporting (Priority: P1)

Uzemeltetoként szeretnem, ha az embedding modell az Intel NPU-n toltodne be es a `/health` endpoint jelezne az embedding szolgaltatas elerehetoseget. Igy a rendszer haromfele hardvert hasznal: dGPU (split stage-1), iGPU (split stage-2) es NPU (embedding).

**Why this priority**: Az NPU kihasznalasa a cel — a chat serving mar foglalja a GPU-kat, az embedding modell kicsi (0.6B int4) es az NPU-ra tervezve alkalmas.

**Independent Test**: A szerver inditasa utan a `/health` endpoint tartalmaz embedding-specifikus mezot ami jelzi az NPU betoltes sikeret es a modell nevet.

**Acceptance Scenarios**:

1. **Given** a profil embedding konfiguracioval rendelkezik, **When** a szerver elindul, **Then** az embedding modell az NPU eszkozre toltodik be es a `/health` valaszban az embedding allapot "ready"
2. **Given** az NPU nem elerheto vagy a modell betoltes sikertelen, **When** a szerver elindul, **Then** a health endpoint "degraded" allapotot jelent az embedding szolgaltatasra, de a chat serving tovabbra is mukodik
3. **Given** a szerver fut, **When** a `/v1/models` endpointot kerdezik, **Then** az embedding modell is megjelenik a modelllistaban a chat modell mellett

---

### User Story 3 - Batch embedding es hasznalati metrikai (Priority: P2)

Fejlesztoként szeretnek batch (tobb szoveg egyidoben) embedding kereseket kuldeni es latni a valaszban a token-felhasznalast, hogy a koltseget es hataridot tervezhessem.

**Why this priority**: A batch funkcio es usage mezo hasznos, de nem blokkolja az alapmukodesst.

**Independent Test**: Egy keresben 10 szoveget kuldunk, a valasz `usage` mezoje tartalmazza a `prompt_tokens` es `total_tokens` ertekeket.

**Acceptance Scenarios**:

1. **Given** a szerver fut, **When** a kliens egy keresben 10 kulonbozo szoveget kuld, **Then** a valasz `data` tombje pontosan 10 embedding objektumot tartalmaz es a `usage.prompt_tokens` nullanal nagyobb
2. **Given** a szerver fut, **When** a kliens ures input szoveget kuld, **Then** a valasz hibat jelez (400 Bad Request) olvashato hibauzenettel

---

### Edge Cases

- Mi tortenik, ha az NPU eszkoz nem tamogatja az adott modell preciziot? A rendszer egyertelmu hibaval jelzi a betoltesi kudarcot, nem csendesen CPU-ra esik vissza.
- Mi tortenik, ha a kliens tulepiti a modell maximum token limitet? A rendszer trunkalt embedding-et ad vagy hibat dob, a viselkedes dokumentalt.
- Mi tortenik, ha egyidejuleg chat completion es embedding keres erkezik? Az ket kulonbozo eszkozcsoporton fut (GPU vs NPU), egymast nem blokkolhatjak.
- Mi tortenik, ha a modellkonyvtar nem letezik vagy serult? A szerver elindul, a chat serving mukodik, de az embedding "failed" allapotot jelez a health-ben.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: A rendszer **MUST** betoltse a Qwen3-Embedding-0.6B-int4-cw-ov modellt az NPU eszkozre az OpenVINO GenAI SDK-n keresztul
- **FR-002**: A rendszer **MUST** kiszolgaljon egy `/v1/embeddings` HTTP POST endpointot, amely az OpenAI Embeddings API szerzodesevel kompatibilis (request: `model`, `input`, `encoding_format`; response: `object`, `data`, `model`, `usage`)
- **FR-003**: A rendszer **MUST** tamogasson egyedi es batch (tomb) inputot egyetlen keresben
- **FR-004**: A rendszer **MUST** visszaadja az embedding vektort `float` tombkent, a modell nativ dimenzioszamaval
- **FR-005**: A rendszer **MUST** jelezze az embedding szolgaltatas allapotat a `/health` endpointban (pl. `embedding_state`, `embedding_model`, `embedding_device`)
- **FR-006**: A rendszer **MUST** az embedding modellt a `/v1/models` valaszban is listazza
- **FR-007**: A rendszer **MUST** fuggetlenul kezelje az embedding pipeline-t a chat pipeline-tol — az embedding betoltesi hiba nem akadalyozhatja a chat servinget
- **FR-008**: A rendszer **MUST** visszautasitsa a csendes CPU fallback-et az embedding modellnel (hasonloan a split profilhoz)
- **FR-009**: A rendszer **MUST** a valasz `usage` mezojeben kozvetitse a `prompt_tokens` es `total_tokens` ertekeket

### Key Entities

- **EmbeddingRequest**: A kliens altal kuldott keres — tartalmazza a modell azonositot, input szoveg(eket) es opcionalis encoding formatumot
- **EmbeddingResponse**: A valasz — tartalmaz `data` tombot (embedding objektumok indexszel es vektorral), `model` azonositot, es `usage` szamlalokat
- **EmbeddingPipelineState**: Az embedding pipeline aktualis allapota — eszkoz, modell, betoltesi eredmeny, hiba informacio

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Egyedi embedding keres valaszideje 200ms alatt marad rovid szovegeknel (1-50 token) a celgepen
- **SC-002**: 10 szoveges batch embedding keres valaszideje 1 masodperc alatt marad
- **SC-003**: Az embedding endpoint valasza megfelel az OpenAI `/v1/embeddings` sema elvarasainak (kliens konyvtarak valtoztatas nelkul hasznalhatok)
- **SC-004**: Az embedding szolgaltatas elerehetosege es hibaja a `/health` endpointban egy lepesbol diagnosztizalhato
- **SC-005**: Az embedding es a chat serving egyidejuleg mukodik anelkul, hogy egymasra hatast gyakorolnanak

## Assumptions

- A celgep Intel Core 7 Ultra 265k processzor, amelynek integralt NPU-ja elerheto az OpenVINO-n keresztul
- Az embedding modell (`Qwen3-Embedding-0.6B-int4-cw-ov`) mar letoltve es elerheto a helyi fajlrendszerben: `C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\models\LLM\embedding\openVINO\OpenVINO---Qwen3-Embedding-0.6B-int4-cw-ov`
- Az NPU eszkoz az OpenVINO-ban "NPU" neven erheto el (device string)
- Az embedding es a chat modell parhuzamos betoltese nem okoz memoria-konfliktust, mivel kulonbozo eszkozoket hasznalnak (NPU vs GPU)
- Az `encoding_format: "base64"` tamogatasa nem elvaras az elso verzioban; `float` az egyetlen tamogatott formatum
- A rendszer egyetlen embedding modellt szolgal ki (nem tobb modell egyidejuleg)
