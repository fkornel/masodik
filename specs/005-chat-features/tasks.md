# Tasks: Chat Funkciok

**Input**: Design documents from `/specs/005-chat-features/`
**Prerequisites**: plan.md, spec.md, research.md

**Organization**: A feladatok user story szerint szervezettek (US1-US7).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Parhuzamosithato
- **[Story]**: Melyik user story-hoz tartozik

---

## Phase 1: Setup

**Purpose**: Konfiguracios fajlok es AGENTS.md letrehozasa

- [x] T001 [P] config/masodik.json bovitese user_name, agent_name, agent_prompt_file, chat_history_file mezokkel a config/masodik.json fajlban
- [x] T002 [P] SYSTEM_PROMPT.md letrehozasa a repo gyokereben — Szilvabot system prompt (nev, cel, nyelv, Kornel neve)
- [x] T003 Konfiguracio olvasas bovitese a src/main/tui_app.cpp fajlban — user_name, agent_name, agent_prompt_file, chat_history_file kiolvasasa a config-bol

---

## Phase 2: Foundational

**Purpose**: Kozos adatszerkezetek es modul interfeszek

- [x] T004 [P] ChatMessage adatszerkezet bovitese az include/masodik/tui/chat_message.hpp fajlban — name mezo (string) hozzaadasa a role melle
- [x] T005 [P] chat_history.hpp/cpp modul letrehozasa az include/masodik/tui/chat_history.hpp es src/tui/chat_history.cpp fajlokban — JSONL append (ofstream ios::app) + load (soronkent JSON parse) + rewrite (teljes ujrairas szerkesztes eseten)
- [x] T006 [P] markdown_render.hpp/cpp modul letrehozasa az include/masodik/tui/markdown_render.hpp es src/tui/markdown_render.cpp fajlokban — render_markdown(string) → ftxui::Element (bold, ujsor, kodblokk, inline kod)

**Checkpoint**: Az adatszerkezetek es modulok lefordulnak.

---

## Phase 3: User Story 1 — Szemelyre szabott nevek (Priority: P1)

**Goal**: [Kornel] es [Szilvabot] nevek a chat-ben a konfigbol olvasva.

- [x] T007 [US1] A chat_screen.cpp renderben a [Te] → [Kornel] es [AI] → [Szilvabot] csere a konfigbol olvasott nevek alapjan — a src/tui/chat_screen.cpp es src/main/tui_app.cpp fajlokban
- [x] T008 [US1] A send_message es start_generation lamdbakban a ChatMessage.name mezo kitoltese a konfiguralt nevekkel a src/main/tui_app.cpp fajlban

---

## Phase 4: User Story 2 — Beviteli mezo javitas (Priority: P1)

**Goal**: Tobbsoros, Shift+Enter, inaktiv generalas kozben.

- [x] T009 [US2] Shift+Enter kezeles: az FTXUI CatchEvent-ben Shift+Enter → uj sor beszuras az input_content-be (Enter tovabbra is kuld) a src/main/tui_app.cpp fajlban
- [x] T010 [US2] Generalas kozben a beviteli mezo inaktiv: ha generating==true, az input_component Render()-je helyett dim szoveget mutatunk es a billentyuzet inputot elnyeljuk a src/main/tui_app.cpp fajlban
- [x] T011 [US2] Hosszu szavak torese: az FTXUI Input multiline + a render_chat_screen-ben paragraph() hasznalata a src/tui/chat_screen.cpp fajlban

---

## Phase 5: User Story 3 — Markdown rendereles (Priority: P1)

**Goal**: A valaszokban **bold**, ujsor, kodblokk, inline kod vizualisan megjelenik.

- [x] T012 [US3] render_markdown() implementalasa a src/tui/markdown_render.cpp fajlban — \n → ftxui::vbox soronkent, **bold** → ftxui::bold, `inline` → ftxui::color(Yellow), ``` kodblokk → ftxui::border + dim
- [x] T013 [US3] A chat_screen.cpp-ben az AI valaszok renderelesenel paragraph() helyett render_markdown() hivas a src/tui/chat_screen.cpp fajlban

---

## Phase 6: User Story 4 — System prompt (Priority: P1)

**Goal**: Szilvabot tudja a sajat nevet es Kornel nevet.

- [x] T014 [US4] SYSTEM_PROMPT.md fajl betoltese a tui_app.cpp-ben — a fajl tartalma string-kent beolvasva a src/main/tui_app.cpp fajlban
- [x] T015 [US4] System prompt hozzaadasa a pipeline hivashoz: a send_message/start_generation-ben a request.messages ele egy {"role":"system","content":agents_md_tartalom} beillesztese a src/main/tui_app.cpp fajlban

---

## Phase 7: User Story 5 — Chat history (Priority: P2)

**Goal**: JSONL mentes es betoltes.

- [x] T016 [US5] Chat history betoltes az alkalmazas indulaskor: a chat_history::load() hivasa a tui_app.cpp-ben, az uzeneteket a chat_state->messages-be toltjuk a src/main/tui_app.cpp fajlban
- [x] T017 [US5] Chat history append: minden uzenet (user + assistant) utan append a JSONL fajlba a src/main/tui_app.cpp fajlban (a send_message es a streaming callback vegen)
- [x] T018 [US5] data/ konyvtar automatikus letrehozasa ha nem letezik a src/main/tui_app.cpp fajlban

---

## Phase 8: User Story 6 — Esc megszakitas (Priority: P1)

**Goal**: Generalas kozben Esc → megall, reszleges valasz megmarad.

- [x] T019 [US6] Az Esc billentyu kezeles bovitese a src/main/tui_app.cpp fajlban: ha generating==true, akkor generating=false (a should_cancel callback true-ra all), a beviteli mezo ujra aktiv
- [x] T020 [US6] A reszleges valasz megorzese: a streaming callback terminal chunk nelkul is megmarad a chat_state->messages-ben, az is_streaming flag false-ra all a src/main/tui_app.cpp fajlban

---

## Phase 9: User Story 7 — Korabbi uzenetek szerkesztese (Priority: P2)

**Goal**: Fel/le kurzor navigacio, Enter betoltes, szerkesztes utani torlés.

- [x] T021 [US7] Uzenet navigacio allapot: browse_index valtozo (-1 = beviteli mezo, 0+ = sajat uzenetek indexe) a src/main/tui_app.cpp fajlban
- [x] T022 [US7] Felfele nyil kezeles: ha a beviteli mezo ures es felfele nyilat nyom, browse_index az utolso sajat uzenetre all, vizualis kiemeles a chat_screen-ben a src/main/tui_app.cpp es src/tui/chat_screen.cpp fajlokban
- [x] T023 [US7] Lefele nyil kezeles: browse_index csokkentese vagy visszateres a beviteli mezore a src/main/tui_app.cpp fajlban
- [x] T024 [US7] Enter a navigacio kozben: a kivalasztott uzenet tartalma betoltodik az input_content-be, browse_index resetelve a src/main/tui_app.cpp fajlban
- [x] T025 [US7] Szerkesztes utani torlés: ha a betoltott uzenet nem az utolso, az utana levo uzenetek (es valaszaik) torlodnek a chat_state->messages-bol es a JSONL fajl teljes ujrairasra kerul a src/main/tui_app.cpp es src/tui/chat_history.cpp fajlokban
- [x] T026 [US7] Esc a navigacio kozben: browse_index = -1, visszateres a beviteli mezore kijeloles nelkul a src/main/tui_app.cpp fajlban

---

## Phase 10: Polish

**Purpose**: Regresszio, edge case-ek, dokumentacio.

- [x] T027 [P] Regresszio ellenorzes: a meglevo tesztek (embedding, split) mind PASS maradnak
- [x] T028 [P] Edge case: serult JSONL fajl → ures chattel indul, hibas markdown → nem omlik ossze
- [x] T029 CLAUDE.md es specs/ frissitese a chat funkciok informacioval

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Nincs fuggoseg — azonnal indithato
- **Foundational (Phase 2)**: Setup-ra var
- **US1 (Phase 3)**: Foundational-ra var — MVP
- **US2 (Phase 4)**: Foundational-ra var, US1-gyel parhuzamosithato
- **US3 (Phase 5)**: Foundational-ra var (markdown_render modul)
- **US4 (Phase 6)**: Setup-ra var (AGENTS.md)
- **US5 (Phase 7)**: Foundational-ra var (chat_history modul)
- **US6 (Phase 8)**: US1 utan (a chat-nek mukodnie kell)
- **US7 (Phase 9)**: US5 utan (history kell a szerkeszteshez)
- **Polish (Phase 10)**: Minden US utan

### Parallel Opportunities

- T001-T003 (Setup) parhuzamosithatoak
- T004-T006 (Foundational) parhuzamosithatoak
- US1, US2, US3, US4 egymastol fuggetlenul implementalhatoak a Foundational utan
- US6 es US7 a megfelo US-ek befejezese utan

---

## Implementation Strategy

### MVP First

1. Phase 1-2: Setup + Foundational
2. Phase 3: US1 — [Kornel] + [Szilvabot] nevek
3. Phase 6: US4 — System prompt
4. **STOP es VALIDACIO**: A nevek es a system prompt mukodik

### Incremental Delivery

1. US1 + US4 → Nevek + system prompt (MVP)
2. US2 → Beviteli mezo javitas
3. US3 → Markdown rendereles
4. US5 → Chat history mentes/betoltes
5. US6 → Esc megszakitas
6. US7 → Uzenet szerkesztes
7. Polish → Regresszio, edge case-ek

---

## Notes

- A nevek a config/masodik.json-bol jonnek (nem kodban rogzitett)
- A system prompt az AGENTS.md fajlbol toltodik be
- A JSONL append-only kivetel: szerkesztes eseten teljes ujrairas
- Az F2 (uj chat) NEM resze ennek a feature-nek
- Kontextus tomorites → kesobbi feature
