# Implementation Plan: 128k Deep Split Serving (US4)

**Branch**: `002-manual-model-split` | **Date**: 2026-04-07 | **Spec**: [spec.md](spec.md)
**Input**: US4 bovites — 128k kontextus ablak deep split-tel

## Summary

A gpt-oss-20b modell 128k kontextus ablakkal valo kiszolgalasa a B580 dGPU + iGPU split konfiguracioval. A jelenlegi 32k tail-split (24 reteg dGPU + tail iGPU) nem skalazhato 128k-ra, mert a KV cache 24 retegnel ~3.2GB, ami a modell sulyaival egyutt tullepi a 12GB VRAM-ot. Megoldas: **melyebb split** — kevesebb reteg a dGPU-n (KV cache csokkenese), a tobbi reteg az iGPU-n (megosztott rendszermemoria, "korlatlan").

## Memoria analisis

### Jelenlegi helyzet (32k tail-split)

| Komponens | Meret | Eszkoz |
|-----------|-------|--------|
| Modell sulya (INT4, 24 reteg) | ~10-11 GB | dGPU (12GB) |
| KV cache (24 reteg, 32k, u8) | ~805 MB | dGPU |
| Aktivacios memoria | ~1.4 GB | dGPU |
| **Osszesen** | **~12.2 GB** | **SZOROS** |

### 128k a jelenlegi split-tel (NEM FER EL)

| Komponens | Meret | Eszkoz |
|-----------|-------|--------|
| Modell sulya (INT4, 24 reteg) | ~10-11 GB | dGPU (12GB) |
| KV cache (24 reteg, 128k, u8) | ~3.2 GB | dGPU |
| Aktivacios memoria | ~5.4 GB | dGPU |
| **Osszesen** | **~18.6 GB** | **6.6 GB TULLEPES** |

### Megoldas: Deep split — reteg ujraelosztás

**Kulcs felismeres**: Az iGPU megosztott rendszermemoriát hasznal (nem dedikalt VRAM). A 64GB rendszermemoria boven eleg a KV cache-nek. A dGPU-ra csak annyi reteg kerul, amennyinek a sulya + KV cache-e elfer 12GB-ban.

**Keplet retegenkent (u8 KV, 128k context):**
- KV/reteg = 2 * 8 * 64 * 131072 * 1 byte = **134 MB/reteg**
- Modell suly/reteg (INT4) = ~420 MB/reteg (10GB / 24)

**dGPU budget (12GB) = N * (420 + 134) MB + overhead**
- N * 554 MB + 1 GB overhead < 12 GB
- N < (12000 - 1000) / 554 = **~19.8 reteg**

**Javasolt deep split konfiguracio:**

| Profil | dGPU retegek | iGPU retegek | dGPU memoria | Context |
|--------|-------------|-------------|--------------|---------|
| 32k tail-split (meglevo) | 0-23 (24) | tail | ~12.2 GB | 32k |
| **128k deep-split (uj)** | **0-17 (18)** | **18-23 + tail (6+tail)** | **~11.0 GB** | **128k** |

A 18-reteg split a dGPU-n:
- Modell suly: 18 * 420 MB = 7.56 GB
- KV cache: 18 * 134 MB = 2.41 GB
- Overhead: ~1 GB
- **Osszesen: ~11.0 GB** → **BELFER a 12 GB-ba, ~1 GB tartalek**

Az iGPU-n (6 reteg + tail, rendszermemoria):
- Modell suly: 6 * 420 MB = 2.52 GB
- KV cache: 6 * 134 MB = 0.80 GB
- **Osszesen: ~3.3 GB** → rendszermemoria, boven belfer

A 18 reteg dGPU + 6 reteg iGPU elosztás optimalis: maximalizalja a dGPU kihasznaltsagat
(kevesebb reteg az iGPU-n = jobb teljesitmeny), mikozben ~1 GB tartalekot hagy.

### Handoff buffer 128k-nal

A hidden state atadasa a ket GPU kozott:
- Per decode step: 1 * 1 * 2880 * 4 bytes = **11.5 KB** (elhanyagolhato)
- Prefill chunking (768 token/chunk): 1 * 768 * 2880 * 4 = **8.85 MB** (managelheto)
- A jelenlegi chunked handoff mechanizmus **megfelelo 128k-hoz** — nem ez a bottleneck

## Technical Context

**Language/Version**: C++23, MSVC (VS 18 BuildTools), Ninja, RelWithDebInfo
**Primary Dependencies**: OpenVINO GenAI SDK 2026.1.0.0, meglevo masodik framework
**Storage**: Uj split artifact generalasa 16-reteg split hatarrral
**Testing**: CTest + unit/integration tesztek + benchmark (128k latency + memoria)
**Target Platform**: Windows 11, Intel Core 7 Ultra 265k, B580 dGPU (12GB) + iGPU (shared memory)
**Performance Goals**: 1-2 tok/s @ 128k context, cold start < 120s, handoff RAM < 500 MB
**Constraints**: 12 GB dGPU VRAM limit, meglevo 32k profil megtartasa, nincs modell ujratanitas
**Scale/Scope**: Uj profil a 128k-hoz, a 32k profil valtoztalan

## Constitution Check

*GATE: PASS*

- **I. Modern C++ and MSVC First**: PASS — valtoztalan toolchain, C++23
- **II. Hardware-Tuned OpenVINO Serving**: PASS — a deep split a hardver korlatokra (12GB VRAM) optimalizalt, az iGPU shared memory kihasznalasa a kulcs
- **III. Measurable Performance and Stability Gates**: PASS — SC-006 (1-2 tok/s), SC-007 (<500MB handoff), SC-008 (<120s cold start) definialva
- **IV. Test, Benchmark, and Regression Discipline**: PASS — 128k benchmark, profil validacio, regresszio a 32k-ra
- **V. Hungarian-First Maintainability**: PASS — magyar kommentek es racionalek

## Project Structure

### Uj fajlok / modositasok

```text
config/profiles/
└── b580-deep-split-128k.json            # UJ: 128k deep split profil

src/split/
└── split_artifact_builder.cpp           # Modositas: rugalmas split hatar tamogatas

tests/
├── integration/
│   └── test_manual_split_128k_deep.cpp  # UJ: 128k deep split integration teszt
├── smoke/
│   └── test_128k_deep_split.ps1         # UJ: valodi 128k smoke teszt
└── fixtures/
    └── deep_split_128k_request.json     # UJ: 128k teszt fixture

benchmarks/
└── steady-state/
    └── benchmark_128k_deep_split.cpp    # UJ: 128k latency benchmark

tools/scripts/
└── generate-manual-split-artifact.ps1   # Modositas: 128k profil tamogatas
```

## Implementacios terv

### Phase 1: Uj 128k profil es artifact generalas

1. Uj profil letrehozasa (`b580-deep-split-128k.json`):
   - `split_stage_1_device: "GPU.1"`, `split_stage_2_device: "GPU.0"`
   - Split hatar: layer 18 (0-17 dGPU, 18-23+tail iGPU)
   - `desired_context_tokens: 131072`
   - `kv_cache_precision: "u8"`
   - `split_stage_1_memory_budget_gb: 12.0`
   - `split_stage_2_memory_budget_gb: 8.0` (shared memory)

2. Split artifact generalas a 16-os reteghatarral:
   ```powershell
   .\tools\scripts\generate-manual-split-artifact.ps1 `
     -OutputRoot .\build\generated-manual-split-cut16-128k-u8 `
     -Profile config\profiles\b580-deep-split-128k.json
   ```

### Phase 2: Validacio es benchmark

1. Szerver inditas az uj profillal
2. Health ellenorzes — mindket stage ready
3. Rovid keres teszt (pár mondat) → valasz erkezik
4. 128k keres teszt (hosszu dokumentum) → valasz erkezik, gep stabil
5. Latency benchmark: tok/s meres 128k-nal
6. Memoria monitoring: peak RAM < fizikai memoria 80%

### Phase 3: Finomhangolas

Ha a 16-reteg split nem optimalis:
- Probaljunk 16 vagy 19 reteget a dGPU-n
- A cel: maximalis retegszam a dGPU-n ami meg belfer a 128k KV cache-sel
- Minden probalkozes: artifact generalas → szerver inditas → benchmark

## Kockazatok es mitigacio

| Kockázat | Valoszinuseg | Mitigacio |
|----------|-------------|-----------|
| A 16 reteg + 128k KV nem fer 12GB-ba | Közepes | Csökkentés 14 rétegre |
| Az iGPU lassusaga dominalja a latency-t | Magas | A 8 reteg iGPU-n lassabb mint 24 reteg dGPU-n, de elfogadhato ha 1-2 tok/s teljesul |
| A prefill 128k tokennél nagyon lassu | Magas | A chunked prefill (768 token) mar implementalva, de 170 chunk × 2 stage = sok infer hivas |
| Rendszermemoria kimerules | Alacsony | 64GB RAM, az iGPU resz ~4.5GB, boven belfer |

## Kovetkezo lepes

`/speckit.tasks` — konkret feladatlista a deep split implementaciohoz
