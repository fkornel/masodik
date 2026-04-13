# Research: Chat Funkciok

**Feature**: 005-chat-features
**Date**: 2026-04-12
**Updated**: 2026-04-13

## 1. Chat history tarolas

**Decision**: JSONL append-only fajl (data/chat-history.jsonl)

**Rationale**: Az NTFS append-only fajl a leggyorsabb megoldas. Az std::ofstream std::ios::app flaggel nyitva tartva a Windows OS azonnal a memoriacacbe-be irja az adatot. Nincs SQL overhead, nincs kulso fuggoseg.

**Alternatives considered**:
- SQLite: Jo lenne kereshetosegre, de tulzott overhead egy egyszeru chat history-hoz.
- JSON tombos fajl: Minden iraskor ujra kell irni az egesz fajlt — lassu nagy history eseten.

## 2. Konfiguracio tarolas

**Decision**: config/masodik.json bovites (user_name, agent_name, agent_prompt_file, chat_history_file, max_tokens, temperature, top_p, top_k, repetition_penalty)

**Rationale**: A meglevo JSON konfiguracio bovitese a legegyszerubb. A generalasi parameterek is ide kerultek, igy a felhasznalo anelkul finomhangolhatja a modell viselkedest, hogy a profil fajlhoz nyulna.

## 3. System prompt tarolas

**Decision**: SYSTEM_PROMPT.md fajl a repo gyokereben (NEM AGENTS.md)

**Rationale**: Az AGENTS.md mar letezett fejlesztoi agenteknek szolo guide-kent. A system prompt kulon SYSTEM_PROMPT.md fajlba kerult, hogy ne utkozzo a fejlesztoi dokumentacioval. A config-ban `agent_prompt_file` mezo mutat ra.

**Alternatives considered**:
- AGENTS.md: Nev-utkozes a meglevo fejlesztoi guide-dal.
- Kodba agyazott string: Nem szerkesztheto kulon.

## 4. Markdown rendereles

**Decision**: Egyszeru regex-mentes parse az FTXUI korlatain belul

**Rationale**: Az FTXUI nem tamogat teljes markdown renderelest. A bold (**szoveg**), ujsor, kodblokk es inline kod elegseges a chat hasznalati esethez. A parse egyszeru string feldolgozas — nincs regex fuggoseg. Inline szegmensek text()-tel renderelodnek (nem paragraph()-tal), mert paragraph() hbox-on belul extra terkozoket okoz.

## 5. Beviteli mezo rendereles

**Decision**: paragraph() alapu sajat rendereles (nem az FTXUI Input default renderelese)

**Rationale**: Az FTXUI Input multiline modban nem tordeli automatikusan a hosszu sorokat (horizontalisan gorget). A paragraph() FTXUI Element viszont szohatarokon toreli a szoveget. A kurzor vizualisan megjelenik mint █ blokk karakter a szoveg vegen (villogas masodpercenkent). Az Input komponens tovabbra is kezeli a billentyuzet inputot.

## 6. Paste detektalas

**Decision**: Idozites-alapu detektalas (20ms ablak)

**Rationale**: Windows ConPTY-n a beillesztett szoveg karakterenkent erkezik az FTXUI-ba (nem egyetlen eventben). Ha az Enter 20ms-on belul erkezik az elozo karakter utan, az beillesztes resze (ujsor), nem kuldes. Ember nem tud 20ms alatt karaktert gepelni majd Enter-t nyomni. Bracketed paste mod is tamogatott (ha az FTXUI egyetlen eventbe csomagolja).

**Alternatives considered**:
- FTXUI bracketed paste: Windows ConPTY-n nem mukodik megbizhatoan (karakterenkenti kezeles).
- Kuldes billentyuvaltas (Ctrl+Enter): UX valtozas, a felhasznalo Enter-t var kuldesre.

## 7. Browse navigacio scroll

**Decision**: focus Element a kijelolt uzenetren

**Rationale**: Az FTXUI yframe a focus dekoratorral ellott elemhez gorget. Browse modban a kijelolt uzenet kap focus-t (inverted + focus), normal modban egy ures elem az uzenetlista aljan (auto-scroll a legfrissebb uzenetekhez). A DOM-szintu focus nem befolyasolja a billentyu-fokuszt.

**Alternatives considered**:
- focusPositionRelative(0, 1): Nem mukodik megbizhatoan hosszu tartalommal ha nincs fokuszalt elem.

## 8. Uzenet szerkesztes megerosites

**Decision**: Ketlepcsos folyam megerosito modalis ablakkal

**Rationale**: Az Enter a browse modban CSAK betolti az uzenetet a mezobe (nem torol semmit). A torles CSAK a kuldes pillanatabn tortenik, es CSAK megerosites utan ("X uzenet visszavonhatatlanul torlodni fog. I/N"). Ez megakadalyozza a veletlen adatvesztest.

**Alternatives considered**:
- Azonnali torles Enter-re: Veletlen adatvesztest okozott (felhasznaloi visszajelzes).
- Torles nelkuli szerkesztes (uj uzenetkent kuldes): Nem felel meg a specnek — a regi valaszok zavart okoznak.
