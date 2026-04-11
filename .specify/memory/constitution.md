<!--
Sync Impact Report
- Version change: template -> 1.0.0
- Modified principles:
  - Principle 1 placeholder -> I. Modern C++ and MSVC First
  - Principle 2 placeholder -> II. Hardware-Tuned OpenVINO Serving
  - Principle 3 placeholder -> III. Measurable Performance and Stability Gates
  - Principle 4 placeholder -> IV. Test, Benchmark, and Regression Discipline
  - Principle 5 placeholder -> V. Hungarian-First Maintainability
- Added sections:
  - Technical Standards
  - Development Workflow
- Removed sections:
  - None
- Templates requiring updates:
  - updated: .specify/templates/plan-template.md
  - updated: .specify/templates/spec-template.md
  - updated: .specify/templates/tasks-template.md
  - updated: .specify/templates/commands/*.md (no files present, no update needed)
- Follow-up TODOs:
  - None
-->

# Magyar OpenVINO C++ Serving Constitution

## Core Principles

### I. Modern C++ and MSVC First
Minden implementacio **MUST** a fejlesztes idopontjaban a Visual Studio Build Tools altal
tamogatott legfrissebb ISO C++ szabvanyra epuljon. Elso osztalyu toolchain az MSVC; ettol
eltero fordito vagy platform csak akkor vezetheto be, ha a terv dokumentalja az uzleti vagy
technikai kenyszert. A kodbazis **MUST** RAII-alapu eroforras-kezelest, tipusbiztos
abstrakciokat, explicit hibakezelest es modern standard library mintakat hasznalni. Ez azert
nem targyalhato, mert a hosszu tavu stabilitas, diagnosztizalhatosag es teljesitmeny ebben a
projektben csak kiszamithato C++ alapokon tarthato fenn.

### II. Hardware-Tuned OpenVINO Serving
A rendszer elso celja a gpt-oss-20b modell hatekony betoltese es kiszolgalasa az aktualis
fejlesztoi/gepi kornyezetre optimalva. Minden modellbetoltesi, inferencia- es
parhuzamositasi dontes **MUST** figyelembe venni a helyi hardver sajatossagait
(CPU-topologia, memoria, NUMA, gyorsitok, OpenVINO device valasztas, szalthasznalat,
cache-eles, warmup). Altalanos, gepfuggetlen megoldas helyett az alapertelmezett megkozelites
**MUST** gepre szabott profilokkal es meresekkel dolgozni. Az optimalizalasnak stabil
kiszolgalast kell adnia, nem csak egyszeri benchmark-csucsot; ennek oka, hogy a projekt
sikere a tartosan gyors es kiszamithato serving viselkedesen mulik.

### III. Measurable Performance and Stability Gates
Minden erdemi valtozas **MUST** konkret, reprodukalhato teljesitmeny- es stabilitasi
elvarasokhoz kotodni. A specifikaciok es tervek **MUST** tartalmazzanak hideg inditasi,
warm allapotbeli, throughput, latency, memoria es hibaturo celokat, tovabba azt is, hogy
milyen hardveren es milyen meresi eljarassal ervenyesek. Olyan allitas, hogy "gyors" vagy
"stabil", meroszam nelkul nem elfogadhato. Ha egy valtozas regressziot okoz, azt csak
indokolt trade-off es jovahagyott uj baseline mellett lehet elfogadni. A rationale: ebben a
projektben a teljesitmeny es uzemeltethetoseg termekkovetelmeny, nem utolagos finomhangolas.

### IV. Test, Benchmark, and Regression Discipline
Minden feature-nek **MUST** tartoznia automatizalt ellenorzeshez, amely lefedi a funkcionis
helyesseget, a hibakezelest es ahol relevans, a benchmark vagy terheleses regressziot. Uj
modellbetoltesi vagy serving utvonal merge elott **MUST** rendelkezzen legalabb egy
fuggetlenul futtathato unit, integracios vagy end-to-end teszttel, valamint a teljesitmeny
szempontjabol kritikus valtozasoknal benchmark-feladattal. Teszt nelkuli kod csak akkor
fogadhato el, ha a terv dokumentalja, miert nem automatizalhato, es milyen mas ellenorzes
zarja ki a kockazatot. Ez azert kotelezo, mert a gyors rendszerekben a csendes regresszio
koltsegesebb, mint a korai validacio.

### V. Hungarian-First Maintainability
A forraskodban levo megjegyzesek, fejlesztoi dokumentacio es uzemeltetesi leirasok
**MUST** magyar nyelven keszuljenek, kiveve ahol kulso technologia vagy API neve miatt az
angol terminus technicus egyertelmubb. A publikus azonosito-nevek maradhatnak technikailag
indokolt angol formaban, de a szandek, korlat es uzemeltetesi kovetkezmeny magyarul legyen
rogzitve. Minden osszetettebb optimalizaciohoz rovid magyar racionalet **MUST** irni. Ez a
szabaly biztositja, hogy a kesobbi fejlesztes es hibakereses helyi csapatban gyors maradjon,
es a gepspecifikus dontesek ne vesszenek el.

## Technical Standards

- Az alap celterulet nativ C++ szolgaltatas vagy konyvtar, amely OpenVINO-ra epitett
  modellbetoltest es inferencia-kiszolgalast valosit meg.
- Az alapertelmezett modellcel a gpt-oss-20b; ettol eltero modell, kvantalasi strategia vagy
  futtatasi topologia csak dokumentalt indokkal vezetheto be.
- A repository szintu terv **MUST** rogzitse a konkret build- es futtatasi lancot Windows
  kornyezetre, beleertve az MSVC toolchaint, a dependency kezelest es a reprodukalhato
  futtatas modjat.
- A projektstrukturanak el kell kulonitenie a publikus interfeszeket, az OpenVINO
  integracios reteget, a serving pipeline-t, a konfiguraciot, a teszteket es a benchmarkokat.
- A logolasnak es metrikaknak eleg informaciot **MUST** adniuk a modellbetoltes, device
  valasztas, memoriaterheles, latency es hibatortenet utolagos elemzesehez.

## Development Workflow

- Minden specifikacio **MUST** legalabb egy onalloan ellenorizheto user story-t tartalmazni,
  amely valos uzleti vagy uzemeltetesi erteket ad.
- A tervezesi fazis **MUST** dokumentalni a hardverfeltetelezeseket, az OpenVINO device
  strategiat, a konkurencia modellt, a memoria-korlatokat es a benchmark modszertant.
- A feladatbontas **MUST** kulon kezelni a funkcionalis implementaciot, a benchmarkolast, a
  diagnosztikai/megfigyelhetosegi munkat es a stabilitasi vedelmeket, ha ezek az adott
  user storyhoz tartoznak.
- Kodreview soran a reviewer **MUST** ellenorizni a szabvanymegfeleleset, a gepspecifikus
  feltetelezesek dokumentaltsagat, a merheto celokat es a regressziok elleni vedelmet.
- Release-re alkalmas allapot csak akkor all fenn, ha a vonatkozo tesztek es benchmarkok
  eredmenyei rogzitettek, ertelmezhetok es osszhangban vannak az elfogadott baseline-nal.

## Governance

Ez az alkotmany a repository minden specifikacioja, terve es feladatlistaja felett all. Minden
pull requestnek, review-nak es implementacios dontesnek **MUST** igazolnia, hogy nincs
ellentmondasban a fenti elvekkel.

Valtoztatasi szabalyok:
- Az alkotmany modositasat ugyanabban a valtoztatasban **MUST** atvezetni a kapcsolodo
  sablonokban es workflow-dokumentumokban.
- **MAJOR** verzioemeles kell, ha elv torlodik vagy a kovetelmeny erdemben visszafele nem
  kompatibilis modon valtozik.
- **MINOR** verzioemeles kell, ha uj elv vagy uj kotelezo governance/quality gate kerul be,
  illetve ha egy elv lenyegesen bovebb, szigorubb lett.
- **PATCH** verzioemeles kell szovegpontositas, megfogalmazasi javitas vagy nem
  szemantikai finomitas eseten.

Megfelelosegi szabalyok:
- A `/speckit.plan` altal keszitett Constitution Check **MUST** minden elvet nevesiteni es a
  tervben ellenorizheto gate-re forditani.
- A `/speckit.tasks` altal keszitett feladatlista **MUST** tartalmazni a szukseges teszt,
  benchmark, diagnosztikai es dokumentacios munkat, ha azt a scope indokolja.
- Minden kivetel vagy kompromisszum **MUST** a plan.md Complexity Tracking reszeben vagy
  egyertelmu, linkelt dontesi dokumentumban szerepeljen.

Eredeti ratifikacio datuma ennek a projektnek: 2026-04-03. A kesobbi modositasoknal a
Ratified datum valtozatlan marad, a Last Amended datum pedig az elfogadas napjara frissul.

**Version**: 1.0.0 | **Ratified**: 2026-04-03 | **Last Amended**: 2026-04-03
