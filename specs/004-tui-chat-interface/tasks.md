# Tasks: TUI Chat Interfesz

**Input**: Design documents from `/specs/004-tui-chat-interface/`
**Prerequisites**: plan.md, spec.md, research.md

**Tests**: Unit tesztek a chat message kezelesre, smoke teszt a TUI inditasra.

**Organization**: A feladatok user story szerint szervezettek (US1: betoltes+chat, US2: funkcio billentyuk, US3: allapotsor, US4: gorgetes).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Parhuzamosithato (kulonbozo fajlok, nincs fuggoseg)
- **[Story]**: Melyik user story-hoz tartozik (US1, US2, US3, US4)
- Pontos fajl utvonalak a leirasokban

---

## Phase 1: Setup

**Purpose**: FTXUI integracio, egyetlen exe letrehozasa, alapstruktura

- [x] T001 FTXUI v6.1.9 FetchContent hozzaadasa a CMakeLists.txt fajlhoz (FTXUI_BUILD_EXAMPLES=OFF, FTXUI_BUILD_DOCS=OFF)
- [x] T002 Egyetlen masodik.exe target letrehozasa a src/main/CMakeLists.txt fajlban — a jelenlegi masodik-launcher.exe-t felvalto target, ami linkeli a masodik::core es ftxui::component, ftxui::dom, ftxui::screen konyvtarakat
- [x] T003 [P] Parancssori argumentum kezeles modositasa a src/main/main.cpp fajlban — --tui (alapertelmezett) es --serve (HTTP szerver mod) elagazas
- [x] T004 [P] TUI app belépési pont létrehozása a src/main/tui_app.hpp es src/main/tui_app.cpp fajlokban — run_tui_app() fuggveny ami a ServiceConfig-ot kapja
- [x] T005 [P] TUI konyvtar structure letrehozasa: include/masodik/tui/ es src/tui/ konyvtarak, CMakeLists.txt GLOB_RECURSE bovitese a src/tui/*.cpp fajlokra

---

## Phase 2: Foundational (FTXUI alapkomponensek)

**Purpose**: Az osszes user story-hoz szukseges FTXUI alapkomponensek

**CRITICAL**: Amig ez a fazis nem kesz, a user story-k nem kezdhetok el.

- [x] T006 ChatMessage adatszerkezet letrehozasa az include/masodik/tui/chat_message.hpp fajlban — role (user/assistant), content, thinking_content, timestamp, is_streaming mezok
- [x] T007 [P] FunctionKeyBinding konfiguracio letrehozasa az include/masodik/tui/function_keys.hpp es src/tui/function_keys.cpp fajlokban — F1-F12, Esc, Page Up/Down, Enter, Shift+Enter billentyukodok es magyar cimkek
- [x] T008 [P] StatusBarInfo adatszerkezet letrehozasa az include/masodik/tui/status_bar.hpp fajlban — modell nev, eszkoz profil, context hasznalat, tok/s, allapot, webszerver statusz
- [ ] T009 [P] Unit teszt: ChatMessage es FunctionKeyBinding tesztek a tests/unit/test_tui_chat_message.cpp fajlban — HALASZTVA

**Checkpoint**: Az FTXUI lefordul, az adatszerkezetek es a billentyuzet konfiguracio kesz.

---

## Phase 3: User Story 1 — Modell betoltes es chat inditas (Priority: P1) MVP

**Goal**: A TUI elindul, mutatja a betoltesi allapotot, majd chat interfeszt biztosit streaming valasszal.

**Independent Test**: masodik.exe --profile ... --model-root ... elindul, betoltesi allapot lathato, chat mukodik.

### Implementation for US1

- [x] T010 [US1] Betoltesi kepernyo (LoadingScreen) implementalasa az include/masodik/tui/loading_screen.hpp es src/tui/loading_screen.cpp fajlokban — FTXUI Renderer, stage-1/stage-2/embedding allapotjelzok, progress bar vagy spinner
- [x] T011 [US1] Pipeline inicializalas integracio a src/main/tui_app.cpp fajlban — LlmPipelineService es EmbeddingPipelineService letrehozasa, betoltesi kepernyo frissitese az inicializalas soran (kulon szalon)
- [x] T012 [US1] Chat kepernyo (ChatScreen) alap implementalasa az include/masodik/tui/chat_screen.hpp es src/tui/chat_screen.cpp fajlokban — FTXUI vbox: uzenet lista (scrollable) + input mezo (Input komponens) + kuldes Enter-rel
- [x] T013 [US1] Kozvetlen LlmPipelineService streaming hivas a src/main/tui_app.cpp fajlban — FTXUI Input + Enter kuldes + streaming callback + tok/s szamitas
- [ ] T014 [US1] Gondolkodas/vegso valasz vizualis elkuonites — HALASZTVA (a pipeline <think> tag-et mar kezeli, a TUI-ban kulon szin kell)
- [x] T015 [US1] Betoltesi hiba kezeles a src/tui/loading_screen.cpp es tui_app.cpp fajlban — piros szin, hibauzenet
- [ ] T016 [US1] Smoke teszt: TUI inditas — HALASZTVA

**Checkpoint**: A TUI elindul, betolt, chat mukodik streaming valasszal.

---

## Phase 4: User Story 2 — Funkcio billentyuk es navigacio (Priority: P1)

**Goal**: F1-F12 funkcio billentyuk, Esc kilepes, Enter kuldes, Shift+Enter uj sor.

**Independent Test**: F1 → surgo, F2 → uj chat, Esc → kilepes megerosites.

### Implementation for US2

- [x] T017 [US2] Funkcio billentyu dispatcher a src/main/tui_app.cpp fajlban — FTXUI CatchEvent-tel F1, F2, F10, F12, Esc, Enter
- [x] T018 [US2] Surgo kepernyo (HelpScreen) — F1 modal, billentyukioosztas tabla, Enter/Esc bezaras
- [x] T019 [US2] F2: Uj chat — uzenetlista torlese, input mezo uritese
- [x] T020 [US2] Esc: Kilepes megerosites — "Biztosan ki szeretnel lepni? (I/N)" modal dialog
- [x] T021 [US2] Enter uzenet kuldes a src/main/tui_app.cpp fajlban
- [x] T022 [US2] Esc/generating=false a generálás megszakitasahoz

**Checkpoint**: Minden funkcio billentyu mukodik, a navigacio teljes billentyuzetrol.

---

## Phase 5: User Story 3 — Allapotsor es diagnosztika (Priority: P2)

**Goal**: Also allapotsor modell/eszkoz/context/tok-s informacioval.

**Independent Test**: Az allapotsor frissul generálás kozben (tok/s), a webszerver statusza lathato.

### Implementation for US3

- [x] T023 [US3] Allapotsor alap — tok/s, web statusz, F12 diagnosztika, F10 webszerver
- [x] T024 [US3] Tok/s szamitas a streaming callback-ben
- [x] T025 [US3] Context total a status_info-ban a profil alapjan
- [x] T026 [US3] F12: Diagnosztika modal — health valasz reszletes megjelenitese
- [x] T027 [US3] F10: HTTP webszerver be/kikapcsolas — register_openai_routes + start/stop

### Change Request: Allapotsor eroforras monitor (CR-001)

> **Szponzori visszajelzes**: A modell neve nem szukseges a lablecben (F12-re tartozik).
> Helyette valos ideju gep eroforras monitor kell: CPU%, dGPU VRAM, iGPU VRAM, RAM —
> grafikus ASCII progress bar-okkal. A context szamlalo legyen valos (novelkedjen a chat soran),
> szazalekos es grafikus megjelenitéssel.

- [x] T037 [P] [US3] ResourceMonitor osztaly — Windows API: GetSystemTimes (CPU), GlobalMemoryStatusEx (RAM), DXGI IDXGIAdapter3 (dGPU/iGPU VRAM)
- [x] T038 [P] [US3] ASCII progress bar render — [████░░░░] formatumu grafikus kijelzes, szin valtozas terheles alapjan (zold→sarga→piros)
- [x] T039 [US3] StatusBar atirias — modell nev eltavolitva, 2 soros layout: CPU+RAM felul, dGPU+iGPU+Ctx+tok/s alul
- [x] T040 [US3] Context szamlalo valos frissites — prompt becslés + streaming token noveles
- [x] T041 [US3] Periodikus eroforras frissites — hatter szal masodpercenkent, FTXUI Post

### Change Request: Think tag vizualis kezeles + UTF-8 fix (CR-002)

> **Szponzori visszajelzes**: A thinking resz ideiglenesen jelenjen meg halvany szinnel
> amig a vegso valasz nem szuletik meg, utana eltunjon. Az emoji/invalid UTF-8 ne
> torje el a chat template-et a kovetkezo keresnel.

- [x] T042 [US1] Streaming callback: <think> blokk → thinking_content, </think> utan → content. Thinking latszik amig nincs vegso valasz.
- [x] T043 [US1] Chat screen: thinking halvany/dim szinnel, vegso valasz normal — thinking elrejtese a vegso valasz erkezesekor
- [x] T044 [US1] Chat history cleanup: <think> blokkok es invalid UTF-8 eltavolitasa a pipeline hivas elott
- [x] T045 [US1] Generating flag: catch blokkban es szal vegen mindig false-ra allitodik

**Checkpoint**: Az allapotsor el es frissul, a webszerver opcionalisan kapcsolhato.

---

## Phase 6: User Story 4 — Chat elozmeny gorgetese (Priority: P3)

**Goal**: Page Up/Down gorgetes a chat elozmenyben.

**Independent Test**: Tobb uzenet utan Page Up → korabbi uzenetek lathatoak.

### Implementation for US4

- [x] T028 [US4] Scrollable chat terulet — FTXUI yframe automatikus gorgetes
- [x] T029 [US4] Page Up/Down — az FTXUI yframe kezeli nativan
- [x] T030 [US4] Felhasznaloi (zold [Te]) es asszisztensi (cyan [AI]) szinek implementalva

**Checkpoint**: A chat elozmeny gorgethetni, a vizualis megkulonboztetes mukodik.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Veglegesites, terminal kezeles, magyar feliritok, regresszio

- [x] T031 [P] Terminal atmeretezés — az FTXUI Fullscreen mod automatikusan kezeli
- [x] T032 [P] Magyar nyelvu feliritok — betoltesi, surgo, allapotsor, hiba mind magyar
- [x] T033 [P] F3: Profil valtas — TOROLVE (egyetlen profil van, nem releváns)
- [x] T034 [P] F5: Modell ujratoltés — pipeline reset + betoltesi kepernyo + kulon szal
- [x] T035 Regresszio ellenorzes: embedding 13/13 PASS, split 0 FAIL, 128k profil teszt javitva
- [x] T036 CLAUDE.md frissitve: TUI inditas (run.ps1), funkciogombok, TUI modul a tablazatban

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Nincs fuggoseg — azonnal indithato
- **Foundational (Phase 2)**: Setup-ra var — BLOKKOLJA az osszes user story-t
- **US1 (Phase 3)**: Foundational-ra var — MVP
- **US2 (Phase 4)**: US1-re var (a chat kepernyonek leteznie kell a funkcio billentyukhoz)
- **US3 (Phase 5)**: US1-re var (az allapotsor a chat kepernyo resze). A CR-001 (T037-T041) az US3 bovitese.
- **CR-001 (T037-T041)**: Az US3 eroforras monitor bovites — a T023-T027 lezarasa utan indulhat
- **US4 (Phase 6)**: US1-re var (a gorgetes a chat teruleten tortenik)
- **Polish (Phase 7)**: Minden user story befejezese utan

### Parallel Opportunities

- T003-T005 (Setup) parhuzamosithatoak
- T006-T009 (Foundational) parhuzamosithatoak
- US3 es US4 parhuzamosithatoak egymassal (kulonbozo komponensek)
- T031-T036 (Polish) parhuzamosithatoak

---

## Implementation Strategy

### MVP First (US1 Only)

1. Phase 1: Setup — FTXUI integracio, egyetlen exe
2. Phase 2: Foundational — adatszerkezetek, billentyuzet config
3. Phase 3: US1 — betoltes + chat + streaming
4. **STOP es VALIDACIO**: A TUI elindul es chat-elni lehet

### Incremental Delivery

1. US1 → Betoltes + chat (MVP)
2. US2 → Funkcio billentyuk (F1 surgo, F2 uj, Esc kilepes)
3. US3 → Allapotsor + F10 webszerver + F12 diagnosztika
4. US4 → Chat gorgetes
5. Polish → Profil valtas, ujratoltés, magyar feliritok, regresszio

---

## Notes

- Az FTXUI FetchContent-tel kerul integrálásra, nincs kulon telepites
- A masodik.exe egyetlen binaris — a --serve mod a korabbi masodik-launcher.exe funkcioja
- A TUI kozvetlenul hivja a C++ pipeline-t, nincs HTTP reteg
- A HTTP webszerver F10-zel opcionalisan bekapcsolhato
- Magyar nyelvu interfesz es kommentek kotelezoak
