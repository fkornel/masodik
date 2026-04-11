# Research: TUI Chat Interfesz

**Feature**: 004-tui-chat-interface
**Date**: 2026-04-08

## 1. TUI library valasztas

**Decision**: FTXUI v6.1.9

**Rationale**: Az FTXUI a legjobb opcio a kovetkezo szempontok alapjan:
- MSVC teljes kompatibilitas (Windows Console API integracio, C++20/23 tamogatas)
- Gazdag komponens keszlet: Input, Text, Scroller, vbox/hbox, frame, border, color
- Funkcio billentyuk (F1-F12), terminal resize, Unicode/UTF-8 (magyar ekezetek)
- Zero kulso fuggoseg (csak STL), statikus linkelhetoseg → egyetlen exe
- CMake FetchContent integracio, aktiv karbantartas

**Alternatives considered**:
- cpp-terminal: Egyszevrűbb, de nincs gazdag widget keszlet (scrollable area, frame, stb.)
- imtui: Nincs nativ MSVC tamogatas, MSYS2 kell
- PDCurses: Mukodne, de elavultabb API, tobb manualis munka
- Windows Console API kozvetlenul: Tul nagy implementacios koltseg

## 2. Alkalmazas architektura

**Decision**: Egyetlen `masodik.exe` binaris, `--tui` (alapertelmezett) es `--serve` moddal.

**Rationale**: A szponzor elvarasa egyetlen exe. A jelenlegi `masodik-launcher.exe` funkcioja beepul a `--serve` modba. A TUI mod az alapertelmezett — a felhasznalo egyszeruen elinditja az exe-t es kap egy chat interfeszt.

**Alternatives considered**:
- Kulon exe (masodik-tui.exe + masodik-launcher.exe): Tobb binaris, bonyolultabb deploy
- Plugin architektura: Tulzott komplexitas egyetlen hardver-specifikus alkalmazashoz

## 3. Inferencia reteg

**Decision**: A TUI kozvetlenul hivja a LlmPipelineService C++ API-jat, nincs HTTP reteg.

**Rationale**: Az HTTP szerver memoriakezelesi overhead-et jelent (JSON serialize/deserialize, HTTP buffer, socket kezeles). A kozvetlen C++ hivas optimalis memoriakezelest es minimalis latency-t biztosit. A HTTP szerver egy funkcio billentyuvel (F10) opcionalisan bekapcsolhato kulso kliensek szamara.

**Alternatives considered**:
- HTTP localhost hivas: Mukodne, de felesleges overhead a lokalis TUI szamara
- IPC (named pipe): Bonyolultabb mint a kozvetlen hivas, nincs elonye

## 4. Streaming megjelenitese

**Decision**: A meglevo `StreamingChunkCallback` mechanizmust hasznaljuk, ami a `<think>` tag-eket is kezeli.

**Rationale**: A TUI streaming megjelenitesehez ugyanazt a callback-et hasznaljuk amit a HTTP SSE streaminghez fejlesztettunk. A `<think>` blokk halvanyabb szinnel jelenik meg, a vegso valasz normal szinnel. Az FTXUI `Renderer` komponense ujrarajzolja a kepernyot minden uj token erkezésekor.
