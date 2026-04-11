# Feature Specification: TUI Chat Interfesz

**Feature Branch**: `004-tui-chat-interface`  
**Created**: 2026-04-08  
**Status**: Draft  
**Input**: User description: "Magyar nyelvu TUI chat interfesz a masodik alkalmazashoz, modell betoltessel, funkcio billentyukkel es billentyuzet-elso navigacioval."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Modell betoltes es chat inditas (Priority: P1)

Felhasznalokent szeretnem a masodik alkalmazast egy terminalalaku feluletrol inditani, latni a modell betoltesi folyamatat (stage-1, stage-2, embedding), majd a betoltes utan azonnal chat-elni a modellel egy szoveg alapu interfeszen keresztul.

**Why this priority**: Ez a TUI alapfunkcioja. A modell betoltes vizualis visszajelzese es a chat interfesz nelkul a TUI nem ad erteket.

**Independent Test**: A TUI elindul, mutatja a betoltesi allapotot (stage-1 ready, stage-2 ready, embedding ready), majd a chat mezo aktiv es a felhasznalo beir egy uzenetet, a modell valaszol, a valasz streamelve jelenik meg.

**Acceptance Scenarios**:

1. **Given** a TUI elindul a megadott profillal, **When** a modell betoltese folyamatban van, **Then** a feluleten latszik a betoltesi folyamat (progress bar vagy allapotjelzo minden stage-hez)
2. **Given** a modell sikeresen betoltodott, **When** a felhasznalo beir egy uzenetet es Enter-t nyom, **Then** a modell valasza streamelve jelenik meg a chat teruleten
3. **Given** a modell betoltese kozben hiba tortenik, **When** a TUI megjeleníti a hibat, **Then** a felhasznalo latja melyik komponens hibazott es mi a hibauzenet

---

### User Story 2 - Funkcio billentyuk es billentyuzetes navigacio (Priority: P1)

Felhasznalokent szeretnem a TUI-t teljes egeszeben billentyuzetrol iranyitani, funkcio billentyukkel (F1-F12) a legfontosabb muveleteket elerhetove tenni, hogy ne kelljen egeret vagy bonyolult parancsokat hasznalni.

**Why this priority**: A billentyuzetes navigacio a TUI lenyege — ez kulonbozteti meg egy sima terminaltol. A funkcio billentyuk gyors hozzaferest adnak a gyakori muveletekhez.

**Independent Test**: A felhasznalo F1-et nyom es megjelenik a surgo, F2-vel uj beszelgetest indit, Esc-kel kilephet.

**Acceptance Scenarios**:

1. **Given** a TUI fut es a chat aktiv, **When** a felhasznalo F1-et nyom, **Then** megjelenik a surgo ablak az osszes elerheto billentyukombinacio listjaval
2. **Given** a TUI fut, **When** a felhasznalo F2-t nyom, **Then** a jelenlegi beszelgetes torlodik es uj ures chat indul
3. **Given** a TUI fut, **When** a felhasznalo Esc-et nyom, **Then** a TUI kerdest tesz fel a kilepesrol es megerositesre var
4. **Given** a chat szovegmezo aktiv, **When** a felhasznalo Enter-t nyom, **Then** az uzenet elkuldesre kerul (Shift+Enter uj sort szur be)

---

### User Story 3 - Allapotsor es diagnosztika (Priority: P2)

Felhasznalokent szeretnem a felso vagy also allapotsorban latni az aktualis rendszerallapotot: melyik modell aktiv, melyik eszkozok hasznaltak, a context ablak kihasznaltsagat es a generálasi sebesseget.

**Why this priority**: A diagnosztikai informacio fontos az uzemelteteshez es a hatekonysag megitelesere, de a chat funkcionalitas nelkul onmagaban nem ad erteket.

**Independent Test**: Az allapotsorban latszik a modell neve, az eszkozok (GPU.1, GPU.0, NPU), a context kihasznaltsag (pl. 2048/131072 token) es a generálasi sebesseg (tok/s).

**Acceptance Scenarios**:

1. **Given** a TUI fut es a modell betoltodott, **When** a felhasznalo ranez az allapotsorra, **Then** latja a CPU terhelest, dGPU/iGPU VRAM es RAM hasznalatot grafikus progress bar-okkal, a context szamlalot es a "Kesz" allapotot
2. **Given** a modell generálás folyamatban van, **When** a felhasznalo ranez az allapotsorra, **Then** latja a jelenlegi token/sec sebesseget, a novekvo context szamlalot (szazalekkal), es a GPU terheles valtozasat valos idoben
3. **Given** a rendszer hibas allapotba kerul, **When** a felhasznalo ranez az allapotsorra, **Then** az allapotsor szine pirosra valt es a hibauzenet latszik

---

### User Story 4 - Chat elozmeny gorgetese (Priority: P3)

Felhasznalokent szeretnem a chat elozményeket visszagorgethni, hogy korabboi kerdeseimet es a modell valaszait ujra elolvashassam.

**Why this priority**: A gorgetés szukseges a hasznalhatosaghoz de nem blokkolja az alap chat funkciot.

**Independent Test**: A felhasznalo Page Up/Page Down vagy nyilbillentyukkel gorget a chat elozmenyben, a szoveg megjelenik.

**Acceptance Scenarios**:

1. **Given** a chat teruleten tobb uzenet van mint ami elfer a kepernoyon, **When** a felhasznalo Page Up-ot nyom, **Then** a korabbi uzenetek lathatova valnak
2. **Given** a felhasznalo felfele gorgetett, **When** uj valasz erkezik a modelltol, **Then** a chat automatikusan az aljara gorget

---

### Edge Cases

- Mi tortenik, ha a terminal ablak tul kicsi (pl. 40 karakter szeles)? A TUI minimalis meret alattal figyelmezteto uzenetet mutat.
- Mi tortenik, ha a modell betoltese kozben a felhasznalo megprobal chat-elni? A beviteli mezo inaktiv betoltes kozben, "Betoltes folyamatban..." uzenettel.
- Mi tortenik, ha a modell generálás kozben a felhasznalo uj uzenetet kuld? Az uj uzenet sorba kerul es a befejezés utan indul.
- Mi tortenik, ha a terminalt atmeretezik futás kozben? A TUI ujrarajzolja magat az uj meretekhez.
- Mi tortenik, ha a modell nem valaszol (timeout)? A TUI timeout utan hibauzenettel jelzi a problemat.
- Mi tortenik, ha a felhasznalo Ctrl+C-t nyom? Tiszta kilepés a generálás megszakitasaval.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: A rendszer **MUST** teljes kepernyos terminal felulettel (TUI) induljon, amely a masodik alkalmazas profiljat hasznalja a modell betolteshez
- **FR-002**: A rendszer **MUST** vizualisan jelezze a modell betoltesi folyamatat (stage-1, stage-2, embedding pipeline allapota kulon-kulon)
- **FR-003**: A rendszer **MUST** a betoltes befejezese utan azonnali szoveges chat interfeszt biztositson, ahol a felhasznalo uzenetet ir es a modell valasza streamelve jelenik meg
- **FR-004**: A rendszer **MUST** funkcio billentyuket (F1-F12) biztositson a legfontosabb muveletekhez (sugo, uj chat, kilepes, stb.)
- **FR-005**: A rendszer **MUST** teljes billentyuzetes navigaciot biztositson — egér hasznalata nem szukseges egyetlen muvelethez sem
- **FR-006**: A rendszer **MUST** allapotsorban folyamatosan jelezze a gep eroforrasait (CPU terheles, dGPU VRAM, iGPU VRAM, RAM — szazalekos es abszolut ertekkel, grafikus ASCII progress bar-okkal), a context kihasznaltsagot (szazalekos es grafikus), es a generalasi sebesseget (tok/s). A modell neve NEM szukseges az allapotsorban — az a diagnosztikai kepernyore (F12) tartozik
- **FR-007**: A rendszer **MUST** magyar nyelvu feliratokat, uzeketeket es surgoet hasznaljon
- **FR-008**: A rendszer **MUST** kezelje a terminal atmeretezeset anelkul, hogy osszeomlana vagy vizualisan hibas lenne
- **FR-009**: A rendszer **MUST** tisztan kezelje a Ctrl+C megszakitast es az Esc billentyut (megerosito kerdes kilepés elott)
- **FR-010**: A rendszer **MUST** tamogassa a chat elozmeny gorgeteset (Page Up/Page Down)
- **FR-011**: A rendszer **MUST** a modell gondolkodas reszet (analysis) vizualisan elkuloniteni a vegso valasztol (osszecsukhato vagy halvanyabb szinnel)
- **FR-012**: A rendszer **MUST** a TUI-bol kozvetlenul, HTTP reteg nelkul vegezni az inferencia-t — a pipeline C++ szinten hivodik, nem REST API-n keresztul
- **FR-013**: A rendszer **MUST** lehetove tenni az OpenAI-kompatibilis HTTP webszerver be- es kikapcsolasat egy funkcio billentyuvel (pl. F10), a webszerver nem indul automatikusan a TUI-val
- **FR-014**: A rendszer **MUST** az allapotsorban jelezni, ha a webszerver aktiv (port szam megjelenites csak aktiv allapotban)
- **FR-015**: A rendszer **MUST** valos idoben monitorozni es az allapotsorban megjeleninteni a rendszer eroforrasait: CPU szazalekos terheles, dGPU VRAM (foglalt/osszes GB), iGPU VRAM (foglalt/osszes GB), RAM (foglalt/osszes GB) — mindegyik grafikus ASCII progress bar-ral es szazalekos kijelzessel
- **FR-016**: A rendszer **MUST** a context szamlalot a chat soran valoban novelni az elhasznalt tokenek szamanak megfeleloen, es szazalekos + grafikus progress bar formaban is megjelenireni

### Key Entities

- **ChatMessage**: Egy uzenet a chat elozmenybol — szerepkor (felhasznalo/asszisztens), tartalom, idopecset
- **TUIState**: A felület aktualis allapota — betoltes/chat/sugo/kilepes mod, aktualis input, gorgetesi pozicio
- **StatusBarInfo**: Az allapotsor tartalma — eroforras monitor (CPU%, dGPU VRAM, iGPU VRAM, RAM), context kihasznaltsag (szazalek + grafikus), tok/s, webszerver allapot
- **FunctionKeyBinding**: Funkcio billentyu hozzarendelés — billentyu (F1-F12), cimke, muvelet

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A TUI elindul es a modell betoltesi folyamat vizualisan kovetheto a kepernoyon, minden stage allapota kulon lathato
- **SC-002**: A betoltes utan a felhasznalo 2 masodpercen belul betudje irni az elso uzenetet
- **SC-003**: A modell valasza streaming modon, tokenenkent jelenik meg a kepernoyon (nem egyszerre a vegen)
- **SC-004**: Minden funkcio billentyu (F1-F12) hasznalaton belul 100ms-en belul reagal
- **SC-005**: A TUI 30 perces hasznalat soran nem omlik ossze es nem fagy le
- **SC-006**: A felhasznalo kizarolag billentyuzetrol kepes az osszes funkciót elerhetni (eger nelkul)
- **SC-007**: A terminalt atmeretezese utan a TUI 1 masodpercen belul ujrarajzolja magat

## Assumptions

- A TUI kozvetlenul hivja a C++ pipeline-t (LlmPipelineService, EmbeddingPipelineService) — nincs HTTP reteg a TUI es a modell kozott, az inferencia hardver-kozeli es memoriakezelés optimalis
- Az OpenAI-kompatibilis HTTP webszerver NEM indul el a TUI-val egyutt — ez egy kulon funkciogombbal (pl. F10) kapcsolhato be/ki, igy a REST endpointok csak igeny eseten fogyasztanak eroforrasokat
- A cel platform Windows terminál (Windows Terminal, cmd.exe, PowerShell) — a TUI-nak ezeken kell mukodnie
- A TUI egyetlen felhasznalot szolgal ki lokalis gepen — nincs tobbfelhasznalós tamogatas
- A meglevo profilok (32k tail-split, 128k tail-split) valtoztatalanul hasznalhatoak
- Az F1-F12 funkcio billentyuk kiosztasa a tervezes soran veglegesitheto
