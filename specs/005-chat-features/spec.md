# Feature Specification: Chat Funkciok

**Feature Branch**: `005-chat-features`  
**Created**: 2026-04-12  
**Status**: Implemented  
**Input**: Chat szemelyre szabas, beviteli mezo javitas, markdown rendereles, system prompt, chat history.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Szemelyre szabott chat nevek (Priority: P1)

Kornel-kent szeretnem, ha a chat ablakban a sajat nevem ([Kornel]) es az AI neve ([Szilvabot]) jelenne meg a jelenlegi [Te] es [AI] helyett, hogy a beszelgetes szemelyesebb es egyertelmubb legyen.

**Why this priority**: Az identitas az alkalmazas alapvetesz felhasznaloi elmenye — minden uzenetnel latszik.

**Independent Test**: Az alkalmazas inditasa utan a felhasznalo uzenete [Kornel] cimkevel, az AI valasza [Szilvabot] cimkevel jelenik meg.

**Acceptance Scenarios**:

1. **Given** az alkalmazas fut, **When** Kornel beir egy uzenetet, **Then** a chat teruleten [Kornel] cimkevel jelenik meg
2. **Given** az AI valaszol, **When** a valasz megjelenik, **Then** [Szilvabot] cimkevel jelenik meg
3. **Given** a chat history betoltodik, **When** a korabbi uzenetek megjelennek, **Then** a nevek helyesen jelennek meg

---

### User Story 2 - Beviteli mezo javitasa (Priority: P1)

Kornel-kent szeretnem, ha a beviteli mezo:
- Automatikusan novekedne ahogy tobb sort irok (max 8 sor, paragraph() alapu rendereles)
- Hosszu szavakat automatikusan eltorne a megjelenitesben
- Alt+Enter-rel uj sort tudnek beszurni (Shift+Enter is tamogatott kitty terminalon)
- Amig a Szilvabot valaszol, ne tudjak uj uzenetet irni (a mezo inaktiv, dim megjelenes)
- Ctrl+U-val a teljes beviteli mezo torolheto
- Tobbsoros szoveg beillesztese (paste) mukodik — idozites-alapu detektalas

**Why this priority**: A beviteli mezo a fo interakcios pont.

**Independent Test**: Tobb soros szoveg bevitele, Alt+Enter uj sor, paste tobb sor. Generalas kozben a mezo nem engedi az irast.

**Acceptance Scenarios**:

1. **Given** a beviteli mezo aktiv, **When** a szoveg tullepi a sor szelesseget, **Then** a mezo automatikusan uj sort kezd (paragraph sortordeles) es a magassaga no (max 8 sorig)
2. **Given** a beviteli mezo aktiv, **When** Kornel Alt+Enter-t nyom, **Then** uj sor kerul a szovegbe (az Enter tovabbra is kuldi az uzenetet)
3. **Given** a beviteli mezo aktiv, **When** Kornel tobb soros szoveget illeszt be a vagollaprol, **Then** a teljes szoveg befogadasra kerul (az Enter a paste-ben ujsorkent ertelmezheto, nem kuldes)
4. **Given** Szilvabot valaszol (generalas folyamatban), **When** Kornel megprobal irni, **Then** a beviteli mezo nem fogad inputot (dim megjelenes, nincs kurzor)
5. **Given** Szilvabot befejezte a valaszt, **When** a generalas veget er, **Then** a beviteli mezo ujra aktiv
6. **Given** a beviteli mezo tartalmaz szoveget, **When** Kornel Ctrl+U-t nyom, **Then** a mezo kiurul
7. **Given** a beillesztett szoveg 3 sornal tobb, **When** a megjelenitest nezzuk, **Then** osszevont formatum latszik: elso sor + "[N sor, M kar.]"

---

### User Story 3 - Markdown rendereles a valaszokban (Priority: P1)

Kornel-kent szeretnem, ha Szilvabot valaszaiban a markdown formazas megjelenne: felkover szoveg (**valami**), ujsorok, kodblokkok vizualisan elkuonulnenek.

**Why this priority**: Az LLM valaszai markdown formatumuak — enelkul a nyers jelolok zavaroak.

**Independent Test**: Szilvabot valasza tartalmaz **felkover** szoveget → vastagitva jelenik meg. Tartalmaz kodot → vizualisan elkuonul. Az \n ujsorként renderelodik.

**Acceptance Scenarios**:

1. **Given** Szilvabot valasza tartalmaz `\n` karaktert, **When** a valasz megjelenik, **Then** ujsor van a megfelelo helyen
2. **Given** Szilvabot valasza tartalmaz `**felkover**` jelolest, **When** a valasz megjelenik, **Then** a szoveg vastagitva (bold) jelenik meg a csillagok nelkul
3. **Given** Szilvabot valasza tartalmaz kodblokk jelolest (``` ``` ```), **When** a valasz megjelenik, **Then** a kod vizualisan elkuonul (keret + dim szin)
4. **Given** Szilvabot valasza tartalmaz inline kodot (`valami`), **When** a valasz megjelenik, **Then** az inline kod sarga szinnel elkuonul

---

### User Story 4 - System prompt (Priority: P1)

Kornel-kent szeretnem, ha Szilvabot tudna a sajat nevet, a felhasznalo nevet, es az alapvetesz viselkedesi elveket — anelkul, hogy minden beszelgetesben el kellene mondanom.

**Why this priority**: A system prompt nelkul Szilvabot nem tudja ki o es ki beszel hozza.

**Independent Test**: Az elso uzenet utan Szilvabot a valaszaban kepes hivatkozni a sajat nevere es Kornel nevere.

**Acceptance Scenarios**:

1. **Given** uj chat indul, **When** Kornel beir egy uzenetet, **Then** a system prompt (SYSTEM_PROMPT.md) automatikusan bekerul a keresbe
2. **Given** a system prompt aktiv, **When** Kornel megkerdezi "Ki vagy?", **Then** Szilvabot kepes valaszolni a sajat neverol es celjairol
3. **Given** a system prompt stilusjegyeket tartalmaz, **When** Szilvabot valaszol, **Then** rovid, termeszetes, 1-3 mondatos valaszokat ad

---

### User Story 5 - Chat history mentes es betoltes (Priority: P2)

Kornel-kent szeretnem, ha az alkalmazas bezarasa utan a beszelgetesi elozmenyek mentesre kerulnenek es az alkalmazas kovetkezo inditasakor automatikusan betoltodnenek, hogy ne vesszenek el a korabbi beszelgetesek.

**Why this priority**: A beszelgetesi elozmeny ertekesek — az ujrainditás ne torlje oket.

**Independent Test**: Kornel beszelget Szilvabot-tal, kilepe, ujrainditja az alkalmazast → a korabbi uzenetek lathatoak.

**Acceptance Scenarios**:

1. **Given** Kornel beszelget Szilvabot-tal, **When** minden uzenet utan, **Then** az uzenet JSONL-be mentesre kerul (append-only)
2. **Given** az alkalmazas ujraindul, **When** a chat kepernyo megjelenik, **Then** a korabbi uzenetek automatikusan betoltodnek
3. **Given** a mentett history fajl serult vagy hianzik, **When** az alkalmazas indul, **Then** ures chattel indul hibauezenet nelkul

---

### User Story 6 - Valasz generalas megszakitasa Esc-kel (Priority: P1)

Kornel-kent szeretnem, ha Szilvabot valaszanak generalasat barmikor megszakithatnam az Esc gomb lenyomasaval.

**Why this priority**: A megszakitas alapvetesz felhasznaloi kontrollt biztosit.

**Independent Test**: Szilvabot generalas kozben Kornel Esc-et nyom → a generalas azonnal megall, az addig generalt resz megmarad.

**Acceptance Scenarios**:

1. **Given** Szilvabot valaszt general (streaming), **When** Kornel Esc-et nyom, **Then** a generalas azonnal megall
2. **Given** a generalas megszakadt Esc-kel, **When** az addig generalt szoveg megjelenik, **Then** a reszleges valasz megmarad a chat-ben (nem torlodik)
3. **Given** a generalas megszakadt, **When** Kornel uj uzenetet ir, **Then** a beviteli mezo ujra aktiv
4. **Given** a generalas megszakadt, **When** a hibakezelo fut, **Then** NEM jelenik meg hibauzenet (a felhasznalo tudatosan szakitotta meg)

---

### User Story 7 - Korabbi uzenetek szerkesztese (Priority: P2)

Kornel-kent szeretnem a korabbi uzeneteimhez visszamenni es szerkeszteni oket.
A fel/le kurzor billentyukkel navigalok az uzeneteim kozott (csak ures beviteli mezobol).
Enter-rel betoltom a kivalasztott uzenetet a beviteli mezobe.
Ha az ujrakuldott uzenet nem az utolso volt, figyelmeztetest kapok a torlesrol.

**Why this priority**: Az uzenet szerkesztes ertekezes, de a chat alapfunkcionalitas es a megszakitas fontosabb.

**Independent Test**: Kornel beir 3 uzenetet. Felfele nyillal visszamegy az elsore, Enter-rel betolti, modositja, Enter-rel ujrakuldi → megerosites utan a 2. es 3. uzenet es a hozzajuk tartozo valaszok torlodnek.

**Acceptance Scenarios**:

1. **Given** a chat-ben van legalabb 1 korabbi uzenet, **When** Kornel felfele nyilat nyom (ures beviteli mezobol), **Then** az utolso sajat uzenete kiemelodik (inverted), a chat odagorget
2. **Given** Kornel navigal, **When** tovabb nyom felfele, **Then** a chat kovetni a kijelolest (scroll)
3. **Given** egy korabbi uzenet ki van emelve, **When** Kornel Enter-t nyom, **Then** az uzenet tartalma betoltodik a beviteli mezobe szerkesztesre (semmi NEM torlodik meg)
4. **Given** Kornel szerkesztette az uzenetet es Enter-rel kuldi, **When** a kivalasztott uzenet utan vannak tovabbi uzenetek, **Then** megerosito ablak jelenik meg: "X uzenet visszavonhatatlanul torlodni fog. I/N"
5. **Given** a megerosito ablakban, **When** Kornel I-t nyom, **Then** a kesobbi uzenetek torlodnek es az uj uzenet elkuldesre kerul
6. **Given** a megerosito ablakban, **When** Kornel N-t vagy Esc-et nyom, **Then** visszater a szerkesztesbe, semmi nem torlodik
7. **Given** Kornel navigal, **When** Esc-et nyom, **Then** a navigacio megszakad, visszater a beviteli mezore
8. **Given** browse mod aktiv, **When** billentyuzet input erkezik, **Then** a beviteli mezo nem fogad inputot (inaktiv, kurzor nem villog)

---

### Edge Cases

- Mi tortenik, ha a beviteli mezo tobb mint 8 sort tartalmaz? A mezo magassaga maximalizalt (8 sor), utana gorgetés.
- Mi tortenik, ha tobb mint 3 soros szoveget irunk/illesztunk? Osszevont megjelenitest kapunk: "elso sor [N sor, M kar.]"
- Mi tortenik, ha a chat history fajl serult? Ures chattel indul, nem omlik ossze.
- Mi tortenik, ha Szilvabot valasza hibas markdown-t tartalmaz (pl. lezaratlan kodblokk)? A rendereles nem omlik ossze — lezaratlan blokk dim+border-rel renderelodik.
- Mi tortenik, ha a system prompt nem fer bele a context ablakba? A system prompt rovid (< 500 token).
- Mi tortenik, ha Kornel Esc-et nyom de nincs generalas es nincs browse? Semmi — az event elnyeleseodik.
- Mi tortenik, ha az uzenet meghaladja a context ablak meretet? Kuldes elott megerosito ablak: "Becsult meret: ~Xk token, limit: Yk token". Lehetoseg: megis kuldes (I) vagy megse (N/Esc/Ctrl+U).
- Mi tortenik, ha a kontextus tullepese utan a pipeline hibat dob? Baratsagos uzenet jelenik meg a technikai hiba helyett.
- Mi tortenik, ha a JSONL fajlbol sorok torlodnek szerkesztes miatt? A fajl ujrairodikk a megmaradt sorokkal (teljes ujrairas).
- Mi tortenik, ha a felhasznalo Esc-kel megszakitja a generalast? A pipeline-bol erkezo cancel kivetel NEM jelenik meg hibauzenetkent.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: A rendszer **MUST** a felhasznaloi uzenetek mellett a [Kornel] nevet megjelenitreni (konfigbol olvasva)
- **FR-002**: A rendszer **MUST** az AI valaszai mellett a [Szilvabot] nevet megjelenitreni (konfigbol olvasva)
- **FR-003**: A rendszer **MUST** a beviteli mezo magassagat automatikusan novelni (max 8 sor, paragraph alapu)
- **FR-004**: A rendszer **MUST** a hosszu szavakat vizualisan eltorni a beviteli mezoben
- **FR-005**: A rendszer **MUST** Alt+Enter lenyomasara uj sort beszurni a beviteli mezoben (Enter kuldi az uzenetet)
- **FR-006**: A rendszer **MUST** a beviteli mezot inaktivva tenni amig a valasz generalasa vagy browse navigacio folyamatban van
- **FR-007**: A rendszer **MUST** az AI valaszokban az `\n` karaktereket ujsorkent renderelni
- **FR-008**: A rendszer **MUST** az AI valaszokban a `**felkover**` jelolest vastagitott szovegkent megjelenitreni
- **FR-009**: A rendszer **MUST** az AI valaszokban a kodblokk jelolest vizualisan elkuonulten megjelenitreni (border + dim)
- **FR-010**: A rendszer **MUST** az AI valaszokban az inline kod jelolest (`valami`) sarga szinnel megjelenitreni
- **FR-011**: A rendszer **MUST** minden chat kereshez automatikusan hozzaadni a SYSTEM_PROMPT.md tartalmat system szerepkorrel
- **FR-012**: A rendszer **MUST** a chat uzeneteket automatikusan menteni minden uzenet utan (JSONL append)
- **FR-013**: A rendszer **MUST** az alkalmazas inditasakor automatikusan betolteni a chat history-t
- **FR-014**: TOROLVE (F2 uj chat nem scope — de mukodik)
- **FR-015**: A rendszer **MUST** az Esc billentyuvel azonnal megszakitani a generalast, hibauzenet nelkul
- **FR-016**: A rendszer **MUST** lehetove tenni a korabbi felhasznaloi uzenetek kozott a fel/le kurzor billentyukkel valo navigaciot (ures beviteli mezobol indulva), scroll kovetéssel
- **FR-017**: A rendszer **MUST** a kivalasztott korabbi uzenet Enter lenyomasara betolteni a beviteli mezobe szerkesztesre (torles nelkul)
- **FR-018**: A rendszer **MUST** a szerkesztett uzenet ujrakuldese elott megerosito ablakot mutatni a kesobbi uzenetek torleserol
- **FR-019**: A rendszer **MUST** Ctrl+U-val a beviteli mezo teljes torlesét tamogatni
- **FR-020**: A rendszer **MUST** tobbsoros beillesztest (paste) tamogatni idozites-alapu detektalassal
- **FR-021**: A rendszer **MUST** kuldes elott ellenorizni a kontextus meretet es figyelmaztetni ha meghaladja a limitet
- **FR-022**: A rendszer **MUST** hosszu felhasznaloi uzeneteket osszevontan megjelenitreni a chat-ben (elso sor + sor/karakter szam)
- **FR-023**: A rendszer **MUST** a generalasi parametereket (max_tokens, temperature, top_p, top_k, repetition_penalty) a masodik.json-bol olvasni

### Key Entities

- **ChatMessage**: Egy uzenet — felhasznalo neve (Kornel/Szilvabot), tartalom, thinking_content, is_streaming
- **SystemPrompt**: Az LLM-nek automatikusan kuldott rendszer-szintu utasitas — SYSTEM_PROMPT.md fajl
- **ChatHistory**: A mentett beszelgetesi elozmeny — JSONL fajl, append-only + rewrite szerkesztesnel

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A [Kornel] es [Szilvabot] nevek helyesen jelennek meg minden uzenetben
- **SC-002**: A beviteli mezo 5 soros szoveget kepes megjelenitreni (paragraph sortoressel)
- **SC-003**: Az Alt+Enter ujsor es az Enter kuldes megkulonboztetese 100%-ban megbizhato
- **SC-004**: A felkover markdown (**szoveg**) vastagitva jelenik meg a valaszokban
- **SC-005**: A chat history mentes es betoltes sikeresen mukodik az alkalmazas ujrainditasa utan
- **SC-006**: A system prompt hatasara Szilvabot kepes megvalaszolni a "Ki vagy?" kerdest
- **SC-007**: A tobbsoros paste teljes egeszeben bekereszesre kerul (nem csak az elso sor)
- **SC-008**: A kontextus-tullepes elott figyelmeztetes jelenik meg

## Assumptions

- A nevek a config/masodik.json-bol jonnek (user_name, agent_name)
- A system prompt a SYSTEM_PROMPT.md fajlbol toltodik be (nem az AGENTS.md-bol — az fejlesztoi guide)
- A chat history fajl: data/chat-history.jsonl (a config-bol olvasva)
- A markdown rendereles az FTXUI szoveges kornyezetben valosul meg (bold, dim, color, border)
- A kodblokk rendereles egyszeru vizualis elkuonites — nem szintaxis kiemeles
- Az F2 (uj chat) funkcio mukodik de NEM resze ennek a feature specifikacionak
- A kontextus tomorites kesobbi fejlesztes — nem scope
- A chat history torlese/archivalasa szinten kesobbi fejlesztes
- A generalasi parameterek (temperature, top_p, stb.) a masodik.json-bol felulirjak a profil ertekeket
- Debug naplo: data/tui-debug.log (fejlesztesi celra, nem produktiv)
