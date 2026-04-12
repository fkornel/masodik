# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projekt áttekintés

Helyi hibrid AI kiszolgáló rendszer C++23 alapon, OpenVINO GenAI SDK-val. A fő cél a gpt-oss-20b modell hatékony kiszolgálása Intel GPU-kon (B580 dGPU + iGPU) kétszakaszos manual split serving-gel. OpenAI-kompatibilis REST API-t nyújt (chat completions, streaming SSE, tool calling).

**Nyelv**: Magyar-first dokumentáció és kommentek. Technikai azonosítónevek angolul maradhatnak.

## Spec-Kit alapú munkamód

A projekt Spec-Kit keretrendszerrel szervezett. **Minden munka a specs/ artifaktokból indul**:

1. Azonosítsd az érintett feature-t a `specs/` alatt
2. Olvasd el minimum a `spec.md` és `tasks.md` fájlokat
3. Keresd ki az érintett contractot, quickstartot és teszteket
4. Csak ezután nyúlj a kódhoz

Source of truth prioritás: `spec.md` > `plan.md` > `tasks.md` > contracts/quickstart > config > kód.

Ha a kért módosítás nem fér bele meglévő feature-be, új Spec-Kit feature-t kell nyitni. Meglévő feature bővítésénél a vonatkozó `specs/<feature>/` artifaktokat is frissítsd.

## Build és futtatás

**Toolchain**: MSVC (VS 18 BuildTools), Ninja, CMake 3.26+, C++23, RelWithDebInfo.

### Alapparancsok (PowerShell)

```powershell
# Környezet konfigurálás
.\tools\scripts\configure-local.ps1

# Build + natív tesztek
.\tools\scripts\build-and-test.ps1

# Quickstart validáció
.\tools\scripts\validate-quickstart.ps1

# Teljes smoke suite
.\tools\scripts\run-smoke-suite.ps1

# Benchmark export
.\tools\profiling\export-benchmark-results.ps1
```

### CMake közvetlenül

```powershell
# Konfigurálás preset-tel
cmake --preset local-msvc-ninja

# Build
cmake --build build

# Összes natív teszt
cmake --build build --target masodik-regression

# Egy adott teszt futtatása
./build/tests/masodik-tests.exe --gtest_filter="TestSuiteName.TestName"

# Specifikus smoke target-ek
cmake --build build --target masodik-manual-split-proof-smoke
cmake --build build --target masodik-streaming-smoke
```

### Manual split artifact generálás

```powershell
.\tools\scripts\generate-manual-split-artifact.ps1 `
  -OutputRoot .\build\generated-manual-split-cut23-tail-32k-u8 `
  -Profile config\profiles\b580-manual-split-cut23-tail-32k-u8.json
```

### TUI indítás (alapértelmezett mód)

```powershell
.\run.ps1
```

A `config/masodik.json` tartalmazza az alapértelmezett profilt és model-root-ot. A TUI közvetlenül hívja a C++ pipeline-t, nincs HTTP overhead.

Funkciógombok: F1 (Súgó), F2 (Új chat), F5 (Újratöltés), F9 (Web be/ki), F10 (Kilépés), F12 (Diagnosztika).

Fejlesztői hot-reload: `.\dev.ps1` — figyeli a build könyvtárat, új exe esetén újraindítja a TUI-t.

### HTTP szerver indítás (--serve mód)

```powershell
.\run.ps1 --serve
```

## Architektúra

### Fő modulok

| Modul | Könyvtár | Felelősség |
|-------|----------|------------|
| API | `src/api/`, `include/masodik/api/` | OpenAI chat contract (request/response DTO-k) |
| Server | `src/server/`, `include/masodik/server/` | HTTP routing, OpenAI handlers |
| GenAI | `src/genai/`, `include/masodik/genai/` | LLM pipeline service, device selection |
| Split | `src/split/`, `include/masodik/split/` | Kétszakaszos manual split, handoff management |
| Runtime | `src/runtime/`, `include/masodik/runtime/` | Startup state machine, request scheduler, readiness |
| Config | `src/config/`, `include/masodik/config/` | Profil/model repository betöltés |
| Diagnostics | `src/diagnostics/` | Logging, error reporting, metrics |
| TUI | `src/tui/`, `include/masodik/tui/` | FTXUI terminal chat interfész, erőforrás monitor |

### Kétszakaszos split serving

A rendszer a modellt két részre vágja:
- **Stage-1** (GPU.1, B580 dGPU): 0-23 réteg, 12 GB budget
- **Stage-2** (GPU.0, Intel iGPU): tail rétegek, 4 GB budget

`reject_cpu_fallback: true` — a profil szigorúan tiltja a csendes CPU fallback-et.

### Belépési pont

- `src/main/launcher_app.cpp` — fő launcher alkalmazás
- `src/main/split_artifact_tool.cpp` — split artifact generáló utility

### Aktív profil

`config/profiles/b580-manual-split-cut23-tail-32k-u8.json` — 32k context, u8 KV cache, port 29011.

Endpointok: `/health`, `/v1/models`, `/v1/chat/completions`

## Tesztstruktúra

- `tests/unit/` — unit tesztek (contract, state machine, validátorok)
- `tests/integration/` — integrációs tesztek (split, streaming, tool-calling, health)
- `tests/smoke/` — PowerShell HTTP API smoke tesztek
- `tests/fixtures/` — JSON request/response fixture-ök
- `benchmarks/` — teljesítménymérés és hardware proof

## Változtatási szabályok

- Ne regresszáld a non-stream, stream, tool-calling vagy request-audit szerződést
- Ne engedj csendes CPU fallback-et split profilban
- Ne gyengítsd a smoke/benchmark proof elfogadási feltételeket
- Ne tekints placeholder szöveget elfogadható modellválasznak valódi split artifact mellett
- Nem triviális teljesítmény-, memória- vagy diagnostics döntéshez írj rövid magyar racionálét
- Meglévő feature bővítésénél a vonatkozó specs/ artifaktokat is frissítsd

## Ellenőrzési sorrend

**Serving/API módosításnál**: specs/001 artifaktok → `src/api/`, `src/server/`, `src/genai/` → unit+integration tesztek → smoke (chat, stream, tool-calling)

**Manual split módosításnál**: specs/002 artifaktok → `src/split/`, `src/genai/`, `src/runtime/`, `config/profiles/` → unit+integration tesztek → smoke (startup, 128k, diagnostics, hardware proof)
