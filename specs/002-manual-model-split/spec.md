# Feature Specification: Manual Model Split Serving

**Feature Branch**: `002-manual-model-split`  
**Created**: 2026-04-05  
**Status**: Active (reopened 2026-04-07 — US4: 128k deep split)  
**Input**: User description: "A gpt-oss-20b modell stabil kiszolgalasahoz szukseg van a modell ket reszre vagasara, ahol az elso resz az elso gyorsiton, a masodik resz a masodik gyorsiton fut, es a ket szakasz kozott a hidden state atadas kezzel tortenik."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Ketlepcsos modellbetoltes (Priority: P1)

Helyi operator-kent olyan kiszolgalasi modot szeretnek, amely a celmodellt ket, egymassal kompatibilis szakaszkent tolti be, hogy a ket gyorsito kozosen vehessen reszt a kiszolgalasban egyetlen keres kiszolgalasa soran.

**Why this priority**: Ez a strategiai alapfeltetel. Ha a modell nem oszlik ket fix futasi szakaszra, akkor a ket gyorsitos kiszolgalas celja nem teljesul.

**Independent Test**: A split profillal inditott szolgaltatas akkor ad erteket, ha sikeresen eleri a ready allapotot, mindket szakaszt aktivnak jelenti, es nem ter vissza egyetlen gyorsitora vagy teljes CPU-futasra.

**Acceptance Scenarios**:

1. **Given** ervenyes ketreszes modellartifact-keszlet all rendelkezesre, **When** a split profillal indul a szolgaltatas, **Then** a rendszer mindket szakaszt betolti, es csak akkor lesz ready, ha a teljes ketlepcsos futasi lanc hasznalhato.
2. **Given** a masodik szakasz hianyzik vagy inkompatibilis, **When** a split profil indul, **Then** a rendszer failed vagy degraded allapotot jelez, es nem allitja magat ready-nek.

---

### User Story 2 - Stabil hosszu kontextusu kiszolgalas (Priority: P2)

Helyi operator-kent azt szeretnem, hogy a split mod stabilan kezeljen nagy kontextusu kerest, hogy a jelenlegi gepen is kiszamithato szolgaltatasi viselkedest kapjak anelkul, hogy a gep teljes memoriatartaleka elfogyna.

**Why this priority**: A split celja nem csak a ket gyorsito bevonasa, hanem a memoria- es stabilitasi problema csokkentese is.

**Independent Test**: Egy reprezentativ hosszu kontextusu keres lefut a split profillal ugy, hogy a szolgaltatas valaszt ad, a gep stabil marad, es a kiszolgalo nem kerul unhealthy allapotba eroforras-kimerules miatt.

**Acceptance Scenarios**:

1. **Given** egy nagy kontextusu keres erkezik a split profilon, **When** a szolgaltatas feldolgozza a kerest, **Then** a teljes futasi ut a ket szakaszon megy vegig, es a szolgaltatas valaszt ad.
2. **Given** a keres memoriaigenye tullepne a biztonsagos uzemeltetesi keretet, **When** a szolgaltatas ezt elore vagy futas kozben felismeri, **Then** szabalyos hibaval megall, es nem hagyja a gepet instabil allapotban.

---

### User Story 3 - Attekintheto stage-diagnosztika (Priority: P3)

Operator-kent es fejlesztokent szeretnem kulon latni, hogy a ket szakasz kozul melyik milyen allapotban van, hogy hiba eseten gyorsan meg tudjam allapitani, a betoltes, az atadas vagy az egyik gyorsito okozza-e a problemat.

**Why this priority**: A ketlepcsos futas csak akkor uzemeltetheto, ha a hibak nem egyetlen homalyos runtime problemakent jelennek meg.

**Independent Test**: Szandekos stage-hiba eseten a health, a statusz-uzenet es a naplok egyertelmuen megnevezik az erintett szakaszt es a hiba tipusat.

**Acceptance Scenarios**:

1. **Given** a ket szakasz kozti atadas meghibasodik, **When** a szolgaltatas feldolgoz egy kerest, **Then** a szolgaltatas unhealthy allapotba kerul, es a diagnosztika jelzi, hogy az atadasi pont hibazott.
2. **Given** az egyik szakasz betoltese sikertelen, **When** indul a szolgaltatas, **Then** a startup riport kulon jelzi az erintett szakaszt, es a szolgaltatas nem jelenti magat teljesen kesznek.

---

### User Story 4 - 128k kontextus ablak deep split-tel (Priority: P1)

Helyi operator-kent szeretnek egy alternativ profilt, amely 128k kontextus ablakot tamogat a B580 dGPU + iGPU split konfiguracioval. A jelenlegi 32k tail-split profil megtartasa mellett ez egy uj, melyebb split, ahol tobb reteget delegalunk az iGPU-ra, hogy a dGPU-n elegendo memoria maradjon a 128k KV cache szamara. A korabbi kiserlet a handoff buffer memoria-robbanasal bukott el (~136GB RAM), ezert memoria-hatekony handoff megvalositas szukseges.

**Why this priority**: A 128k context a mai LLM alkalmazasok (RAG, hosszu dokumentum elemzes, kod-review) alapfeltetele. A jelenlegi 32k limit komolyan korlatozza a hasznalhatosagot, kulonosen az OpenClaw agent rendszerben, ahol a teljes chat history a promptba kerul.

**Independent Test**: Egy 128k kontextusu keres (pl. hosszu dokumentum + kerdes) sikeresen vegigfut az uj profilon, valaszt ad, es a gep stabil marad. A token generalasi sebesseg legalabb 1 tok/s.

**Acceptance Scenarios**:

1. **Given** az uj 128k deep split profil aktiv es a rendszer ready, **When** egy ~100k token hosszu promptot kuldunk, **Then** a szolgaltatas valaszt ad, es a peak RAM hasznalat nem haladja meg a fizikai memoria 80%-at.
2. **Given** az uj profil fut, **When** a keresek sorozatban erkeznek, **Then** a handoff buffer memoria hasznalata stabil marad (nem no keresrol keresre), es par szaz MB alatt marad.
3. **Given** a regi 32k tail-split profil, **When** parhuzamosan letezik az uj 128k profil, **Then** a ket profil fuggetlen artifact-keszletet hasznal, es barmelyikkel indithat a szerver.
4. **Given** az uj profil es 128k context, **When** a modell general, **Then** a token generalasi sebesseg legalabb 1 token/masodperc.

---

### Edge Cases

- Mi tortenik, ha a 128k context-nel a KV cache nem fer el a dGPU 12GB VRAM-jaban? A rendszer egyertelmu hibaval jelzi es nem csendes CPU fallback-et hasznal.
- Mi tortenik, ha a handoff buffer memoria-igenye tullepi a biztonsagos keretet 128k-nal? A rendszer szabalyos hibaval megall.
- Mi tortenik, ha a deep split reteghatarat rosszul valasztjuk meg es az iGPU tulterhelodik? A profil validacio idejeben jelezni kell.
- Mi tortenik, ha a ket modellresz elteto retegszammal, nem egyezo hidden-state merettel vagy nem kompatibilis metadata-val keszul?
- Mi tortenik, ha az elso szakasz sikeresen betolt, de a masodik nem, es mar reszleges memoriafoglalas tortent?
- Mi tortenik, ha a warmup sikeres, de az elso valos keresnel az atadasi pont hibazik?
- Mi tortenik, ha hosszu kontextus eseten a stage-ek kozti pufferek nagyobbra nonek a biztonsagos keretnel?
- Mi tortenik, ha egyszerre tobb keres erkezik, es a stage-ek feldolgozasi uteme elter egymastol?
- Mi tortenik, ha a split profil inditasakor a rendszer csak egyetlen gyorsitot er el?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: A rendszer MUST tamogatni egy ketreszes modellartifact-keszletet, amely egy elso es egy masodik futasi szakaszt ir le ugyanahhoz a celmodellhez.
- **FR-002**: A rendszer MUST kulon validalni a ket modellresz kompatibilitasat inditas elott, beleertve a reteghatar, a bemeneti-kimeneti tenzorok es a kapcsolodo metadata egyezeset.
- **FR-003**: A split profil MUST csak akkor lephet ready allapotba, ha mindket futasi szakasz sikeresen betoltodott es a stage-ek kozti atadasi lanc ellenorzotten hasznalhato.
- **FR-004**: A rendszer MUST az elso futasi szakaszt az elso kijelolt gyorsiton, a masodik futasi szakaszt a masodik kijelolt gyorsiton futtatni normal uzemben.
- **FR-005**: A rendszer MUST minden keresnel atadni az elso szakasz kimeneti allapotat a masodik szakasznak ugy, hogy a keresek allapota ne keveredjen egymassal.
- **FR-006**: A rendszer MUST fenntartani a meglevo kliens-kompatibilis chat-kiszolgalasi szerzodest a split profil hasznalata mellett is.
- **FR-007**: A rendszer MUST tamogatni a celmodellt hosszu kontextusu keresekkel is a split profilon, mikozben betartja az uzemeltetesi memoria- es stabilitasi korlatokat.
- **FR-008**: A rendszer MUST egyertelmuen visszautasitani a split profil inditasat, ha a futas valojaban egyetlen gyorsitora vagy teljes CPU-feldolgozasra esne vissza.
- **FR-009**: A rendszer MUST kulon stage-szintu diagnosztikat szolgaltatni a betoltesrol, a health allapotrol, a memoriahelyzetrol, a keresek atadasarol es a hibakrol.
- **FR-010**: A rendszer MUST unhealthy vagy failed allapotba valtani, ha barmelyik stage vagy a koztes atadas futas kozben ervenyes keres kiszolgalasat akadályozza.
- **FR-011**: A rendszer MUST kezelni a stage-ek kozti eroforras-eltolodast ugy, hogy tulterheles eseten szabalyos visszajelzest adjon, ne pedig hangtalanul beragadjon.
- **FR-012**: A rendszer MUST lehetove tenni, hogy a split profil uzemeltetesi riportja megmutassa, melyik stage aktiv, melyik stage hibazott, es hogy tortent-e sikeres vegigfutas a teljes ketlepcsos lancban.
- **FR-013**: A rendszer MUST tamogatni tobbfele split profilt (pl. 32k tail-split es 128k deep-split) parhuzamosan, kulon artifact-keszletekkel.
- **FR-014**: A rendszer MUST megvalositani memoria-hatekony hidden state handoff-ot, amely 128k context-nel is par szaz MB RAM-mal mukodik (nem a teljes tensor masolasa).
- **FR-015**: A rendszer MUST tamogatni a retegek rugalmas elosztasat a ket gyorsito kozott, ahol a split hatar profilonkent kulonbozo lehet (pl. 0-12 dGPU + 13-27+tail iGPU).

### Key Entities *(include if feature involves data)*

- **Split Model Artifact Set**: A celmodell ket osszetarto reszbol allo artifact-keszlete, amely meghatarozza a stage-hatart es a kompatibilitasi adatokat.
- **Execution Stage**: A ketlepcsos kiszolgalas egyik fix futasi resze, sajat betoltott allapottal, sajat gyorsitoval es sajat diagnosztikai statuszokkal.
- **Inter-Stage Handoff**: Az a keresenkenti atadasi pont, ahol az elso stage kimeneti allapota a masodik stage bemeneteve valik.
- **Split Serving Profile**: Az a futtatasi profil, amely kijeloli a ket stage-et, a hozzajuk tartozo gyorsitokat, es a split modhoz tartozo stabilitasi korlatokat.
- **Stage Diagnostic Snapshot**: Egy adott idopillanat uzemeltetesi kepe a ket stage allapotarol, memoriahelyzeterol, terheleserol es esetleges hibairol.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A split profil a celgepen a folyamat inditasatol szamitva legfeljebb 90 masodpercen belul ready allapotba kerul, ha a ket stagehez szukseges artifact-ek ervenyesek.
- **SC-002**: Egy reprezentativ 128k kontextusu tesztkeres a split profilon sikeresen vegigfut ugy, hogy a szolgaltatas valaszt ad, es a gep uzemkepes marad kulon kezi beavatkozas nelkul.
- **SC-003**: Egy 60 perces stabilitasi futas alatt a split profil a sikeresen elinditott keresek legalabb 99%-at restart nelkul lekezeli, es nem lep ki unhealthy allapotba eroforras-kimerules miatt.
- **SC-004**: Split profilos stage-hiba eseten az operator egyetlen health-lekerdezesbol es a kapcsolodo naplobol 2 percen belul meg tudja allapitani, hogy az elso stage, a masodik stage vagy a koztes atadas hibazott.
- **SC-005**: A split profil uzeme alatt a gep legalabb 8 GB szabad rendszermemoriat megoriz a reprezentativ terheles soran, igy a teljes rendszer nem valik instabilla.
- **SC-006**: A 128k deep split profil 1-2 tok/s sebessegel general valaszt 100k+ kontextusu kereseknel.
- **SC-007**: A 128k deep split handoff buffer RAM-igenye nem haladja meg az 500 MB-ot egyetlen keres feldolgozasa soran.
- **SC-008**: A 128k profil cold start-tol ready-ig legfeljebb 120 masodpercen belul elindul.

## Assumptions

- A kezdeti scope a gpt-oss-20b modell ketreszes felosztasara fokuszal; a nagyobb modellek tamogatasa kesobbi kiterjesztes lehet.
- A feature a jelenlegi egygepes, helyi kiszolgalasi modot celozza; elosztott vagy tobb gepes futas nincs scope-ban.
- A meglevo kliensoldali chat-szerzodes valtozatlanul megmarad, a feature fo valtozasa a belso futasi modell.
- A celkornyezet tovabbra is a jelenlegi egygepes fejlesztoi gep, ket elerheto gyorsitoval es a mar hasznalt helyi fejlesztoi munkafolyamattal.
- A split modellartifact-ek eloallitasa ennek a feature-nek resze lehet, de a feature elso merfoldkove mar akkor is ertekes, ha a rendszer legalabb egy kezelt, ervenyes ketreszes artifact-keszlettel mukodik.
