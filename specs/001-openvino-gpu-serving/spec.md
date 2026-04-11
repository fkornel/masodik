# Feature Specification: Helyi hibrid AI kiszolgalas

**Feature Branch**: `001-openvino-gpu-serving`  
**Created**: 2026-04-03  
**Status**: Draft  
**Input**: User description: "A 001-es feature celja a mar letoltott helyi modell ujrafelhasznalasaval egy OpenAI-kompatibilis helyi kiszolgalas, ahol a reszleges GPU+CPU gyorsitas kozvetlen modellbetoltessel es sajat szerverreteggel valosul meg."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Helyi AI API eleres (Priority: P1)

Mint helyi kliens vagy automatizalt workflow fejlesztoje, szeretnem a mar hasznalt helyi modellre
egy OpenAI-kompatibilis kiszolgalason keresztul kuldeni a kereseimet, hogy a meglevo eszkozeimet
minimalis atallitassal tovabb tudjam hasznalni.

**Why this priority**: Ez adja a feature kozvetlen felhasznaloi erteket, mert a modell nem csak
helyi eszkozkent, hanem altalanosan elerheto helyi szolgaltataskent is hasznalhato.

**Independent Test**: Egy mar meglevo helyi kliens a szervercim atallitasa utan kepes kerest
kuldni, es szerkezetileg hasznalhato valaszt kap ugyanazon a gepen.

**Acceptance Scenarios**:

1. **Given** a szolgaltatas fut es a helyi modell elerheto, **When** egy kliens ervenyes
    szoveggeneralasi kerest kuld, **Then** a rendszer sikeres valaszt ad vissza kompatibilis
    valaszformaban.
2. **Given** a szolgaltatas fut, **When** a kliens ugyanazt a kerest valos ideju kimenetet kerve kuldi
   el, **Then** a rendszer OpenAI-kompatibilis esemenyfolyamban kezdi el visszakuldeni a reszvalaszokat,
   majd egyertelmuen lezarja a valaszfolyamot.
3. **Given** a szolgaltatas fut, **When** a kliens tamogatott modell-listazast vagy allapotellenorzest
   ker, **Then** a rendszer ertelmezhetoen visszaadja az aktiv helyi modellhez tartozo informaciot.
4. **Given** a kliens hibas vagy hianyos kerest kuld, **When** a szolgaltatas feldolgozza azt,
   **Then** a rendszer kapcsolatszakadas helyett ertelmezheto hibavalaszt ad.

---

### User Story 2 - Uzembiztos helyi futas (Priority: P2)

Mint uzemelteto vagy fejleszto, szeretnem a szolgaltatast a mar rendelkezesre allo helyi modelllel
stabilan elinditani, ujrainditani es hibakeresni, hogy a napi hasznalat ne kezi probalkozasokra
epuljon.

**Why this priority**: A helyi API csak akkor ertekes, ha megbizhatoan elindul, uzemkesz allapotot
jelez, es a hibak gyorsan elkulonithetoek.

**Independent Test**: A szolgaltatas egyertelmuen kulonbseget tesz az indulas alatti, uzemkesz es
sikertelen allapotok kozott, valamint hibas konfiguracio eseten ertelmezheto magyar nyelvu
uzemeltetesi informaciot ad.

**Acceptance Scenarios**:

1. **Given** a szukseges helyi modellcsomag rendelkezesre all, **When** az uzemelteto elinditja a
   szolgaltatast, **Then** a rendszer uzemkesz allapotba kerul es ezt kulon ellenorizheto modon jelzi.
2. **Given** a konfiguracio hianyos, a modell nem toltheto be, vagy a gyorsitasi profil nem
   ervenyesitheto, **When** az uzemelteto elinditja a szolgaltatast, **Then** a rendszer nem marad
   csendes hibas allapotban, hanem egyertelmu hibainformaciot ad.
3. **Given** a szolgaltatas mar futott korabban, **When** az uzemelteto leallitja majd ujrainditja,
   **Then** a rendszer megismetelten uzemkesz allapotba tud kerulni kezi helyreallitas nelkul.

---

### User Story 3 - Hibrid gyorsitas stabil kiszolgalassal (Priority: P3)

Mint uzemelteto, szeretnem, hogy a szolgaltatas a helyi gep tobb szamitasi egyseget is ki tudja
hasznalni, ha ezzel a modell elindithato es kiszolgalhato marad, hogy a gep eroforrasai valos
erteket adjanak a helyi AI hasznalatban.

**Why this priority**: Ez a feature fo kulonbsege a kesz helyi eszkozhoz kepest: a modell ne csak
futtathato legyen, hanem a hardverhez igazodva, stabilan es atlathatoan legyen gyorsithato.

**Independent Test**: A rendszer kepes hybrid gyorsitasi profillal elindulni vagy azt egyertelmuen
elutasitani, es terheles alatt sem valt at csendesen egy, a celzott hasznalhatot elronto altalanos
degradalt uzemmodra.

**Acceptance Scenarios**:

1. **Given** a hybrid gyorsitasi profil elerheto a celgepen, **When** a szolgaltatas azzal indul,
   **Then** a rendszer uzemkesz marad es a diagnosztika lathatova teszi az aktiv profilt.
2. **Given** a hybrid gyorsitasi profil nem alkalmazhato, **When** a szolgaltatas inditasa megtortenik,
   **Then** a rendszer egyertelmu hibaval vagy nem uzemkesz allapottal reagal, nem rejtett altalanos
   lassu uzemmoddal.
3. **Given** tobb keres erkezik egymas utan vagy kisebb parhuzamossaggal, **When** a szolgaltatas
   terheles alatt fut, **Then** a rendszer megorzi az allapotjelzest es nem omlik ossze.

---

### User Story 4 - Tool calling OpenAI-kompatibilis szerzodessel (Priority: P1)

Mint agentikus kliens vagy workflow-orchestrator fejlesztoje, szeretnem, hogy a helyi szolgaltatas
OpenAI-kompatibilis tool calling szerzodest is tamogasson, hogy a modell strukturalt eszkozhivasokat
adhasson vissza, ne csak szabad szoveges valaszokat.

**Why this priority**: A helyi modell agentikus hasznalata csak akkor er valodi uzleti erteket, ha a
kliensoldali orchestrator megbizhatoan meg tudja kapni a `tool_calls` strukturat, kulonosen akkor is,
ha a kliens streaming modot var.

**Independent Test**: Egy tool-definiciot is tartalmazo keresnel a szolgaltatas a modell dontese
eseten OpenAI-kompatibilis `tool_calls` payloadot ad vissza non-stream es stream uzemmodban is,
anélkül hogy a hivas elveszne vagy sima szoveges essay-ve lapulna.

**Acceptance Scenarios**:

1. **Given** a kliens ervenyes `tools` tombbel es `tool_choice` beallitassal kuld chat kerest,
   **When** a modell tool hivas mellett dont, **Then** a rendszer strukturalt `tool_calls` valaszt ad
   vissza OpenAI-kompatibilis mezokkel.
2. **Given** a kliens `stream: true` mellett kuld toolos kerest, **When** a modell tool hivas mellett
   dont, **Then** a rendszer a tool call informaciot SSE delta esemenyekben kozvetiti, es a stream nem
   vesziti el a hivas szerkezetet.
3. **Given** a kliens toolos kerest kuld, **When** a rendszer feldolgozza azt, **Then** a szolgaltatas
   nem hajtja vegre szerveroldalon a toolt, hanem a kliens szamara visszaadja a meghivando eszkoz
   strukturalt leirasat.
4. **Given** a kliens hibas vagy nem tamogatott tool definiciot kuld, **When** a szolgaltatas
    ellenorzi a kerest, **Then** ertelmezheto hibavalaszt ad kapcsolatszakadas vagy csendes fallback
    helyett.
5. **Given** OpenClaw-szeru kliensintegracio mellett nem egyertelmu, hogy a keres tenylegesen
   tartalmazott-e `tools` vagy `tool_choice` mezot, **When** az uzemelteto a diagnosztikat vizsgalja,
   **Then** a rendszer request logging alapjan utolag is meg tudja allapitani, hogy a tool-calling
   szerzodes bemeneti oldalon egyaltalan aktiv volt-e.

### Edge Cases

- Mi tortenik, ha a helyi modellcsomag fizikailag jelen van, de betoltes kozben hibasnak bizonyul?
- Hogyan viselkedik a rendszer, ha a hybrid gyorsitasi profil csak reszben vagy egyaltalan nem aktivalhato?
- Mi tortenik, ha inditas alatt mar erkezik klienskeres, de a szolgaltatas meg nem uzemkesz?
- Hogyan jelzi a rendszer, ha a helyi gep memoria- vagy kapacitas-hatarhoz kozelit?
- Mi tortenik, ha a keressor hosszu, de a szolgaltatas meg mukodokepes marad?
- Hogyan reagal a rendszer, ha egy futo keres ideje alatt a szolgaltatas leallitasra vagy ujrainditasra kerul?
- Hogyan viselkedik a rendszer, ha a kliens `stream: true` kervennyel indit valaszt, de a kapcsolat
  a folyam kozben megszakad vagy a kliens megszakitja az olvasast?
- Mi tortenik, ha a modell streaming kozben reszletekben kezdi visszaadni egy tool hivasanak argumentumait?
- Hogyan kell kezelni azt az esetet, amikor a modell egyszerre ad reszleges szoveges tartalmat es tool-call szandekot?
- Mi tortenik, ha a kliens toolos kerest kuld, de a modell vegul sima szoveges valaszt ad?
- Hogyan jelezze a rendszer, ha a streamelt tool-call payload szerkezetileg hibas vagy megszakad?
- Hogyan allapithato meg utolag, hogy egy OpenClaw altal kuldott keres valojaban tartalmazott-e
  `tools` tombot, es milyen `tool_choice` ertekkel erkezett?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: A rendszernek kepesnek kell lennie a mar helyben rendelkezesre allo modellcsomag
  ujrafelhasznalasara uj modellletoltes kotelezo lepese nelkul.
- **FR-002**: A rendszernek helyi, OpenAI-kompatibilis szoveggeneralasi szolgaltatast kell nyujtania,
  amelyhez a mar meglevo kliensek minimalis atallitassal tudnak kapcsolodni.
- **FR-003**: A rendszernek kulon ellenorizheto modon kell jeleznie az indulas alatti, uzemkesz,
  terhelt es hibas allapotokat.
- **FR-004**: A rendszernek tamogatnia kell legalabb egy olyan futasi profilt, amely a helyi gep
  gyorsitasi lehetosegeit a modell betolthetosegenek es kiszolgalhatosaganak javitasara hasznalja.
- **FR-005**: A rendszernek lathatova kell tennie, melyik futasi profil aktiv, es hogy a szolgaltatas
  milyen uzemeltetesi allapotban van.
- **FR-006**: A rendszernek ervenyes keres eseten hasznalhato valaszt, hibas vagy nem tamogatott
  keres eseten pedig ertelmezheto hibavalaszt kell adnia.
- **FR-007**: A rendszernek tulterheles alatt is meg kell oriznie a stabil mukodest, es tamogatnia
  kell a hosszu varakozast is elfogado, nem interaktiv munkaterheleseket.
- **FR-008**: A rendszer nem valthat at csendesen egy olyan degradalt uzemmodra, amely a celzott
  gyorsitasi vagy stabilitasi elvarast lenyegesen megszegi.
- **FR-009**: A rendszernek ujrainditas utan is kovetkezetesen kepesnek kell lennie az allapot
  ujrafelepitesere vagy a hibak egyertelmu jelzesere.
- **FR-010**: A rendszernek a helyi gepen valasztott halozati kotesi modhoz igazodva konfiguralhato
  elerhetoseget kell adnia.
- **FR-011**: A rendszernek a szoveggeneralasi kereseknel tamogatnia kell a nem streamelt es a
  valos ideju folyamkent visszaadott valaszmodot is.
- **FR-012**: Ha a kliens `stream: true` ertekkel kuld chat kerest, a rendszernek OpenAI-kompatibilis
  szerver-oldali esemenyfolyamkent kell visszaadnia a reszvalaszokat, es a valaszfolyamot egyertelmuen
  le kell zarni.
- **FR-013**: A streamelt valaszokat ugyanahhoz az aktiv modellhez es kereshez kell kotni, mint a
  nem streamelt valaszokat, es a ket uzemmod kozott nem terhet el a kereservenyesites vagy a
  hibavalaszok szemantikaja.
- **FR-014**: Ha a streamelt valasz nem indithato el vagy futas kozben megszakad, a rendszernek ezt
  a kliens szamara egyertelmuen jeleznie kell csendes elakadas vagy vegtelen varakozas helyett.
- **FR-015**: A rendszernek tamogatnia kell az OpenAI-kompatibilis `tools` es `tool_choice` mezoju
  chat completion kereseket.
- **FR-016**: Ha a modell tool hivast valaszt, a rendszernek a nem streamelt valaszban strukturalt
  `tool_calls` tombot kell visszaadnia, es a `finish_reason` mezonek ezt egyertelmuen jeleznie kell.
- **FR-017**: Ha a kliens `stream: true` mellett kuld toolos kerest es a modell tool hivas mellett dont,
  a rendszernek a tool call informaciot OpenAI-kompatibilis SSE delta esemenyekben kell kozvetitenie.
- **FR-018**: A rendszernek a streamelt tool-call delta-k soran meg kell oriznie a tool call indexet,
  a function nevet es az argumentumok fokozatosan epulo tartalmat ugy, hogy a kliens ezeket ossze tudja
  fuzni vegleges hivassa.
- **FR-019**: A szolgaltatas **MUST NOT** hajtson vegre szerveroldalon a kliens altal megadott toolt;
  a v1 tool calling feladata a strukturalt modellkimenet tovabbitasa, nem a tool-execution.
- **FR-020**: A rendszernek toolos kereseknel is meg kell oriznie a non-stream es stream uzemmod kozotti
  szemantikai osszhangot, kulonosen a hibavalaszok, finish reason es lezarasi szabalyok teren.
- **FR-021**: A rendszernek diagnosztikai request logginggal lathatova kell tennie, hogy egy bejovo
  chat completion keres tartalmazott-e `tools` mezot, milyen `tool_choice` ertekkel erkezett, es a
  request tool-calling szempontbol melyik feldolgozasi agra kerult.

### Key Entities *(include if feature involves data)*

- **Model Package**: A helyben elerheto modellhez tartozo fajlok es leiro adatok osszessege,
  amelyek alapjan a szolgaltatas elindithato.
- **Serving Profile**: Az a futasi profil, amely meghatarozza az aktiv gyorsitasi, kapacitasi es
  uzemeltetesi viselkedest.
- **Generation Request**: A kliens altal kuldott szoveggeneralasi keres, annak bemeneteivel es
  elvart valaszparametereivel.
- **Streaming Response Session**: Egy olyan aktiv valaszfolyam, amely egyetlen generalasi kereshez
  tartozik, reszleges kimeneteket tovabbit, majd egy vegleges lezaro allapottal zarul.
- **Tool Definition**: A kliens altal atadott strukturalt eszkozleiras, amely nevvel, leirassal es
  parameter-schema-val jellemzi az elerheto toolt.
- **Tool Call**: A modell altal visszaadott strukturalt eszkozhivasi szandek, amely legalabb tool
  azonosito, function nev es argumentum payload mezoket tartalmaz.
- **Tool Call Stream Delta**: Egy olyan SSE resz-esemeny, amely egy tool call fokozatosan epulo
  szerkezeti adatait kozvetiti.
- **Request Audit Record**: Egy diagnosztikai bejegyzes, amely a bejovo keres azonositojat, a
  `stream` flaget, a `tools` jelenletet, a `tool_choice` alakjat es a valasztott feldolgozasi agat
  rogziti.
- **Runtime Session**: Egy aktiv szolgaltatasi futas, amelyhez allapot, diagnosztikai es
  kapacitasi informaciok tartoznak.
- **Health Snapshot**: Az a lekerdezheto allapotkep, amely a szolgaltatas aktualis uzemkeszseget,
  terhelesi allapotat es fobb hibajelzeseit irja le.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Egy mar letezo helyi kliens legfeljebb a szervercim es a modellazonosito
  beallitasa utan kepes sikeres kerest kuldeni a szolgaltatasnak.
- **SC-002**: A szolgaltatas a celgepen legfeljebb 5 percen belul uzemkesz allapotba kerul a
  mar helyben rendelkezesre allo modellcsomaggal.
- **SC-003**: Legalabb 20 egymas utani szoveggeneralasi keresbol legalabb 19 sikeresen teljesul
  ugyanazon szolgaltatasi futas alatt ujrainditas nelkul.
- **SC-004**: Legalabb 5 egyideju klienskeres mellett a kereses 95%-a vagy sikeres valasszal zarul,
  vagy egyertelmu sorban allasi allapotot kap csendes elakadas nelkul.
- **SC-005**: Hybrid gyorsitasi profil inditasi sikertelensege eseten az uzemelteto legfeljebb
  1 percen belul meg tudja allapitani, hogy konfiguracios, kapacitasi vagy modellbetoltesi problema
  tortent.
- **SC-006**: Ha a celzott gyorsitott profil nem ervenyesitheto, a rendszer legfeljebb egy keresnyi
  kesleltetessel egyertelmu hibajelzest vagy nem uzemkesz allapotot ad, nem rejtett lassu
  degradaciot.
- **SC-007**: A szolgaltatas legalabb 60 perces folyamatos helyi hasznalat alatt megorzi az
  allapotjelzesi kepesseget es nem igenyel kezi ujraepitest rutinszeru kereskuldes mellett.
- **SC-008**: A `stream: true` keressel kuldott sikeres generalasok legalabb 95%-anal a kliens a
  valasz kezdetet es a szabalyos folyamlezarast is megkapja csendes elakadas nelkul.
- **SC-009**: Tool callingot igenylo regresszios forgatokonyveknel a tool hivast valaszto generalasok
  legalabb 95%-a strukturalt `tool_calls` payloadkent jelenik meg, nem veszti el a hivast es nem
  alakul at sima ures vagy narrativ szoveges valassza.
- **SC-010**: Streaming tool calling scenarioknal a kliens a tool call delta-kat ugy kapja meg, hogy a
  function nev es argumentumok osszeallithatoak legyenek egy ervenyes vegleges hivasba.
- **SC-011**: Request logging alapjan az OpenClaw-szeru regresszios vizsgalatok legalabb 95%-anal
  egyertelmuen megallapithato, hogy a keres kuldott-e `tools` mezot, milyen `tool_choice` ertekkel,
  es a szerver tool-calling vagy content-only agra sorolta-e.

## Assumptions

- Az elso verzio egyetlen helyi gepre es egyetlen elsoleges modellvaltozatra optimalizal.
- A modellcsomag mar helyben rendelkezesre all, es annak ujrafelhasznalasa megengedett.
- A szolgaltatas elsoleges felhasznalasa helyi vagy megbizhato helyi halozati kornyezetben tortenik.
- A v1-ben nincs kulon alkalmazasszintu hitelesites; a futtatasi kornyezet helyi bizalmi peremnek
  szamit.
- A fo felhasznalasi minta nem interaktiv, hanem hosszu timeoutot is elfogado, automatizalt vagy
  agentikus futtatas.
- A helyi kliensek kozott vannak olyan integraciok, amelyek a valaszokat OpenAI-kompatibilis
  streamelt esemenyfolyamkent varjak, ezert a valos ideju valaszmod v1 scope-ba tartozik.
- A tool callingot hasznalo kliensek a szervertol OpenAI-kompatibilis tool-call payloadot varnak, de a
  tenyleges tool vegrehajtast tovabbra is a kliensoldali orchestrator intezi.
- A feature fo erteke a stabil helyi kiszolgalas es a hybrid gyorsitas kiprobalhatosaga, nem a
  tobbgepes vagy felhos skala.
