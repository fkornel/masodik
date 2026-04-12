# Implementation Plan: TUI Chat Interfesz

**Branch**: `004-tui-chat-interface` | **Date**: 2026-04-08 | **Spec**: [spec.md](spec.md)
**Input**: Magyar nyelvu TUI chat interfesz kozvetlen C++ pipeline hivassal, FTXUI library-vel.

## Summary

Egyetlen `masodik.exe` binaris, amely a TUI chat interfeszt es az opcionalis HTTP webszervert egyarant tartalmazza. A TUI kozvetlenul hivja a C++ LlmPipelineService-t es EmbeddingPipelineService-t (nincs HTTP overhead). Az FTXUI library biztositja a terminal feluletkezelest. A jelenlegi `masodik-launcher.exe` funkcioja beleolvad az uj binarisba.

## Szponzori elvarasok (rogzitett)

- MSVC (VS 18 BuildTools) fordito, C++23
- Egyetlen exe kimenet (masodik.exe)
- Nincs Python, JavaScript
- Magyar kommentek es interfesz
- Hardver-specifikus optimalizacio (Intel B580 + iGPU + NPU, Core 7 Ultra 265k)
- Kozvetlen C++ inferencia, HTTP reteg opcionalis

## Technical Context

**Language/Version**: C++23, MSVC (VS 18 BuildTools), Ninja, RelWithDebInfo
**TUI Library**: FTXUI v6.1.9 (CMake FetchContent, statikus linkelés, 0 kulso fuggoseg)
**Primary Dependencies**: OpenVINO GenAI SDK 2026.1.0.0, FTXUI, Windows Console API
**Storage**: Profilok, modellek, logok — valtoztalan a meglevo rendszerhez kepest
**Testing**: CTest + unit tesztek, manualis TUI tesztek, meglevo smoke scriptek
**Target Platform**: Windows 11, Intel Core 7 Ultra 265k, B580 dGPU + iGPU + NPU
**Project Type**: Nativ C++ alkalmazas (egyetlen exe)
**Constraints**: Egyetlen binaris, nincs HTTP overhead a TUI-bol, MSVC only

## Constitution Check

*GATE: PASS*

- **I. Modern C++ and MSVC First**: PASS — C++23, MSVC, FTXUI modern C++ library
- **II. Hardware-Tuned OpenVINO Serving**: PASS — kozvetlen pipeline hivas, nincs HTTP reteg
- **III. Measurable Performance and Stability Gates**: PASS — SC-001-007 definialt
- **IV. Test, Benchmark, and Regression Discipline**: PASS — unit + smoke + meglevo tesztek
- **V. Hungarian-First Maintainability**: PASS — magyar interfesz es kommentek

## Architektura

### Egyetlen binaris: masodik.exe

```
masodik.exe
  ├── main()
  │   ├── --tui mod (alapertelmezett): TUI chat interfesz
  │   └── --serve mod: HTTP szerver (korabbi masodik-launcher funkcio)
  │
  ├── TUI reteg (FTXUI)
  │   ├── BetoltesKepernyo — stage-1/stage-2/embedding allapot
  │   ├── ChatKepernyo — uzenetek + input mezo + allapotsor
  │   ├── SugoKepernyo — F1 surgo (billentyukioosztas)
  │   └── FunkcioGombok — F1-F12 kezeles
  │
  ├── Pipeline reteg (kozvetlen C++ hivas)
  │   ├── LlmPipelineService — chat inferencia
  │   └── EmbeddingPipelineService — embedding (NPU)
  │
  └── Opcionalis HTTP reteg (F10 billentyuvel be/ki)
      └── HttpServer + OpenAI routes (meglevo kod)
```

### Funkcio billentyuk terv

| Billentyu | Muvelet | Leiras |
|-----------|---------|--------|
| F1 | Surgo | Billentyukioosztas megjelenites |
| F2 | Uj chat | Jelenlegi beszelgetes torlese, uj inditasa |
| F3 | Profil valtas | 32k / 128k profil kozott valtas |
| F5 | Modell ujratoltés | Pipeline ujrainicializalas |
| F9 | Webszerver be/ki | HTTP endpoint inditasa/megallitasa |
| F10 | Kilepes | Megerosito kerdes utan kilepes (univerzalis) |
| F12 | Diagnosztika | Health allapot reszletes megjelenitese |
| Esc | Modal bezaras | Modal ablakok bezarasa (NEM kilepes) |
| Ctrl+C | Hint | Hint uzenet: "Hasznald az F10-et" (nem lep ki) |
| Page Up/Down | Gorgetes | Chat elozmeny gorgetese |
| Enter | Kuldes | Uzenet kuldese |
| Shift+Enter | Uj sor | Tobbsoros uzenet bevitele |

### FTXUI integracio

```cmake
include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v6.1.9
)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# Egyetlen exe — a TUI es a szerver egyarant benne van
add_executable(masodik
    src/main/main.cpp
    src/main/tui_app.cpp
    src/main/launcher_app.cpp  # HTTP szerver mod
)
target_link_libraries(masodik PRIVATE
    masodik::core
    ftxui::component ftxui::dom ftxui::screen
)
```

### Kepernyo elrendezes

```
┌─────────────────── masodik v0.1.0 ──────────────────┐
│ ┌─────────────────────────────────────────────────┐  │
│ │ [Te] Mi a fotema ennek a dokumentumnak?         │  │
│ │                                                  │  │
│ │ [AI] Gondolkodas...                             │  │
│ │  > A dokumentum fo temaja a...                  │  │
│ │  > Harom fo pontot emel ki:                     │  │
│ │  > 1. ...                                       │  │
│ │                                                  │  │
│ └─────────────────────────────────────────────────┘  │
│ ┌─────────────────────────────────────────────────┐  │
│ │ Ird be az uzeneted...                           │  │
│ └─────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────┤
│ CPU[████░░░░]35% RAM[██████░░]12/64GB               │
│ dGPU[████████]10/12GB iGPU[██░░░░░░]2/4GB           │
│ Ctx[██░░░░░░]2k/128k 2% | 3.2 tok/s | Kesz         │
│ F1:Surgo F2:Uj F3:Profil F10:Web F12:Diag Esc      │
└──────────────────────────────────────────────────────┘
```

## Project Structure

### Uj es modositott fajlok

```text
src/main/
├── main.cpp                  # MODOSITAS: --tui / --serve mod valasztas
├── tui_app.cpp               # UJ: TUI foalkalmazas
├── tui_app.hpp               # UJ: TUI fejlec
├── launcher_app.cpp          # MEGLEVO: HTTP szerver mod (valtoztalan)
└── CMakeLists.txt            # MODOSITAS: FTXUI + egyetlen exe

include/masodik/tui/
├── chat_screen.hpp           # UJ: Chat kepernyo komponens
├── loading_screen.hpp        # UJ: Betoltes kepernyo
├── help_screen.hpp           # UJ: Surgo kepernyo
├── status_bar.hpp            # UJ: Allapotsor + eroforras monitor
├── resource_monitor.hpp      # UJ: CPU/GPU/RAM valos ideju monitor
└── function_keys.hpp         # UJ: Funkcio billentyu kezeles

src/tui/
├── chat_screen.cpp           # UJ: Chat kepernyo implementacio
├── loading_screen.cpp        # UJ: Betoltes kepernyo
├── help_screen.cpp           # UJ: Surgo
├── status_bar.cpp            # UJ: Allapotsor + eroforras bar-ok
├── resource_monitor.cpp      # UJ: Windows API alapu eroforras lekerdezes
└── function_keys.cpp         # UJ: Funkcio billentyuk

tests/
├── unit/test_tui_chat_message.cpp  # UJ: Chat uzenet kezeles tesztek
└── smoke/test_tui_startup.ps1      # UJ: TUI inditas smoke teszt

CMakeLists.txt                # MODOSITAS: FTXUI FetchContent + masodik exe
```

## Implementacios lepesek

### Phase 1: FTXUI integracio + alapfelulet
1. FTXUI FetchContent hozzaadasa a CMakeLists.txt-hez
2. Egyetlen `masodik.exe` letrehozasa (TUI + szerver mod)
3. Betoltesi kepernyo (stage-1, stage-2, embedding allapot)
4. Minimalis chat kepernyo (input mezo + valasz terulet)

### Phase 2: Chat funkcionalitas
1. Kozvetlen LlmPipelineService hivas a TUI-bol
2. Streaming valasz megjelenitese (tokenenkent)
3. Gondolkodas/vegso valasz vizualis elkuonites
4. Chat elozmeny gorgetese (Page Up/Down)

### Phase 3: Funkcio billentyuk + allapotsor
1. F1-F12 billentyukezeles
2. Allapotsor (modell, eszkoz, context, tok/s)
3. F10: HTTP webszerver be/kikapcsolas
4. F2: Uj chat, Esc: kilepes

### Phase 4: Polish
1. Terminal atmeretezés kezeles
2. Magyar nyelvu feliritok veglegesitese
3. Smoke tesztek
4. Regresszio ellenorzes

## Kovetkezo lepes

`/speckit.tasks` — konkret feladatlista az implementaciohoz
