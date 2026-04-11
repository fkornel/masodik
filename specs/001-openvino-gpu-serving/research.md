# Research: Helyi hibrid AI kiszolgalas

## Kutatasi kiindulopont

A cel egy olyan helyi, OpenAI-kompatibilis kiszolgalas, amely:
- a mar telepitett helyi OpenVINO runtime-ot es a mar letoltott modellt hasznalja,
- nem tolt le uj modellt,
- a helyi MSVC toolchainre epul,
- a B580 GPU + CPU hybrid profil kiprobalasat kozvetlen modellbetoltessel teszi lehetove,
- kulso modellserver helyett sajat C++ szerverreteggel dolgozik.

## Decision: A helyi MSVC toolchain pontos pinelese

**Decision**: A build toolchain a helyi MSVC 14.50.35717, a `cl.exe` a Visual Studio Build Tools
telepitesebol jon.

**Rationale**: Ez felel meg a projekt alkotmanyanak es a helyi gep valos allapotanak.

## Decision: Minimalis build tooling a Visual Studio Build Tools-bol

**Decision**: Nem kell kulon CMake/Ninja telepites; a Visual Studio Build Tools komponensei
elegendok.

**Rationale**: A helyi gepen a `cmake.exe`, `ctest.exe` es `ninja.exe` mar elerheto.

## Decision: CTest az alap tesztfuttato

**Decision**: A natív C++ tesztfuttatas CTest + sajat teszt executable-k alapon marad.

**Rationale**: Nem kell kulon kulso tesztfugges, es ez mar mukodo baseline a repositoryban.

## Decision: A mar letoltott modell kotelezo ujrafelhasznalasa

**Decision**: A projekt tovabbra is a mar helyben levo `OpenVINO---gpt-oss-20b-int4-ov`
modellrepot hasznalja.

**Rationale**: Ez teljesiti a "ne toltsunk le uj modellt" feltetelt, es a helyi modell mar
elerheto a gepen.

## Decision: Nincs uj runtime-letoltes alapertelmezeskent

**Decision**: A mar telepitett helyi OpenVINO runtime a baseline; uj runtime letoltest csak akkor
szabad bevonni, ha az audit bizonyitja, hogy a GenAI C++ fejlesztoi artefaktumok hianyoznak.

**Rationale**: A helyi bundle-ben a runtime DLL-ek jelen vannak, ezert a fejlesztesnek elobb a
helyi allapotot kell pontosan felmernie.

## Decision: In-process OpenVINO GenAI, nem kulso modellserver

**Decision**: A rendszer az OpenVINO GenAI iranyra valt, es a modell a sajat folyamaton belul
toltodik be.

**Rationale**: A korabbi kulso modellserveres iranynal a hybrid GPU+CPU profil nem bizonyult
jarhatonak; az in-process megkozelites nagyobb szabadsagot ad az eszkozkiosztashoz es a
diagnosztikahoz.

## Decision: Sajat OpenAI-kompatibilis helyi szerver

**Decision**: A kulso szerzodes helyi OpenAI-kompatibilis chat/model/health felulet marad, de ezt
most mar a sajat C++ szerver reteg szolgalja ki.

**Rationale**: A kliensoldali kompatibilitast meg kell tartani, mikozben a belso runtime ut
szabadon valthat.

## Decision: Nincs csendes lassu degradacio

**Decision**: Ha a kivant hybrid gyorsitott profil nem tarthato, a rendszer nem valthat at csendben
egy olyan altalanos lassu uzemmodra, amely elrejti a problemat.

**Rationale**: A projekt fo erteke a stabil, atlathato gyorsitasi kiserlet; a csendes degradacio
diagnosztikai szempontbol elfogadhatatlan.

## Decision: 32768 context validacios cel marad

**Decision**: A 32768 context-cel tovabbra is kotelezo validacios cel.

**Rationale**: Ez a valos terhelesi igenyhez igazodik, es ugyanazzal a mar letoltott modellel
vizsgalhato.

## Decision: A tool calling szerveroldalon csak strukturalt modellkimenet legyen

**Decision**: A szolgaltatas tool calling tamogatasa a kliens felol kapott `tools` es `tool_choice`
mezok feldolgozasara, valamint a modell altal visszaadott `tool_calls` payload strukturalt
tovabbitasara terjedjen ki. A szerver maga nem hajt vegre toolt.

**Rationale**: Ez illeszkedik az OpenAI-kompatibilis backend szerephez, minimalizalja a biztonsagi
kockazatot, es lehetove teszi, hogy az OpenClaw-szeru orchestrator maradjon a tenyleges tool executor.

**Alternatives considered**:
- szerveroldali tool vegrehajtas: tul nagy biztonsagi es scope-bovitesi kockazat
- tool calling teljes kihagyasa: az agentikus felhasznalasi mintakhoz nem eleg

## Decision: A teljes streaming tool calling legyen cel, ne implicit fallback

**Decision**: A tervezesi cel teljes streaming tool calling tamogatas legyen, ahol a kliens
`stream: true` es toolos keres mellett is strukturalt, fokozatosan epulo `tool_calls` SSE delta-kat kap.

**Rationale**: A felhasznaloi cel konkretan agentikus, streamingre epulo kliensintegraciokat erint.
Az implicit non-stream fallback kisebb kockazatu lenne, de nem oldana meg a kliensoldali elvarast ott,
ahol maga a kliens defaultkent streamelt OpenAI-kompatibilis valaszokra epit.

**Alternatives considered**:
- automatikus non-stream fallback toolos kereseknel: kisebb kor, de nem teljes kompatibilitas
- explicit hiba `stream: true` + `tools` kombinacion: egyszerubb implementacio, de funkcionalis hiany

## Decision: A streaming tool call delta-k fokozatosan epulo szerkezetkent legyenek kezelve

**Decision**: A tool call streaming utvonalnak kulon kell kezelnie az indexet, a function nevet es az
argumentum sztring fokozatosan epulo reszeit, es ezek osszeallithatosagat regresszios teszteknek kell
vedeniuk.

**Rationale**: A streaming kliens nem feltetlenul egyetlen komplett JSON objektumot kap, hanem delta-kat.
Ha a szerver nem tartja fenn a szerkezeti kovetkezetesseget, a tool call gyakorlati erteke elveszik.

**Alternatives considered**:
- teljes tool call egyetlen terminalis chunkban: egyszerubb, de nem valodi streaming
- arguments mezot nyers szovegkent kezelni szerkezeti garancia nelkul: kliensoldalon torzekeny

## Decision: Kulon regresszios vedelmet kell epiteni OpenClaw-szeru klienshibak ellen

**Decision**: A tool callinghoz kulon non-stream es streaming regressziokat kell felvenni, kulonosen
arra az esetre, amikor a kliens a streamelt valaszban elveszti a `tool_calls` payloadot vagy ures
content valaszt kap.

**Rationale**: A valos probléma nem csak elmeleti: ismert kliens/backend kombinacioknal a streaming
tool call elveszhet. A helyi szervernek ezt explicit tesztekkel kell vedeni, kulonben a feature
hallgato regrediálhat.

**Alternatives considered**:
- csak manualis OpenClaw proba: nem eleg regressziovedelmi szint
- csak non-stream tool tesztek: nem vedik a kritikus streaming utat

## Decision: A lathato reasoning kimenet jelenleg nem szerzodeses felulet

**Decision**: A jelenlegi tervezesi korben a fo feladat a tool calling szerzodes es a strukturalt
valaszok bevezetese. A modell altal lathato reasoning-szeru szoveg kulonszureset kulon,
kesobbi kompatibilitasi finomitasnak tekintjuk.

**Rationale**: A reasoning-szeru kimenet model- es promptfuggo, mig a tool calling strukturaja
egyertelmu API-szerzodesi feladat. A ket problemat egyszerre megoldani foloslegesen noveli a kockazatot.

**Alternatives considered**:
- azonnali reasoning-szures a tool callinggal egy korben: nagyobb torzitesi es regresszios kockazat
- reasoning teljes ignorálasa dokumentalas nelkul: felreertest okoz a kulso klienseknek

## Decision: A request logging strukturalt audit legyen, ne teljes promptdump

**Decision**: A kovetkezo korben a szerver request loggingja kulon audit bejegyzesben rogzitse, hogy a
bejovo keres tartalmazott-e `tools` mezot, hany tool definiciot kuldott, milyen `tool_choice`
alakban erkezett, streamelt-e, es a szerver melyik feldolgozasi agra sorolta. A teljes prompt- vagy
argumentumtartalom csak kontrollalt, roviditett vagy opcionális debug mezokent jelenhet meg.

**Rationale**: Az OpenClaw-szeru hibakereses fo kerdese itt nem a token-szintu modellvalasz, hanem az,
hogy a kliens egyaltalan aktiv tool-calling szerzodessel kuldte-e a kerest. Ehhez strukturalt audit
elég, es kisebb adat- es zajterhelest okoz, mint a teljes request body logolas.

**Alternatives considered**:
- teljes raw request body logolas: egyszerubbnek tunik, de tul zajos es adatminimalizalasi szempontbol gyengebb
- semmilyen request logging: nem adna bizonyitekot arra, hogy a kliens kuldott-e `tools` vagy `tool_choice` mezot

## Decision: A request auditnak a tool-calling dontesi agat is rogzitenie kell

**Decision**: A request logging ne csak a bemenetet (`tools` jelenlet, `tool_choice`), hanem a szerver
donteset is rogzitse legalabb `content_only`, `tool_call_auto`, `tool_call_required`, `tool_call_named`
vagy ezekkel egyenerteku kategoriakban.

**Rationale**: Ha csak a nyers bemenet latszik, nem derul ki, hogy a szerver parser-validacio utan
milyen agra kuldte a kerest. Az OpenClaw hibakeresesehez mind a bemenet, mind a valasztott szerveragi
dontes fontos.

**Alternatives considered**:
- csak a parsereredmeny logolasa: nem eleg, mert a heuristikus vagy explicit tool-call dontes ekkor meg elveszhet
- csak a vegso finish_reason logolasa: nem eleg, mert pre-stream vagy validacios hiba eseten nincs vegso valasz

## Nyitott technikai kerdesek

- A helyi AI Playground bundle pontosan tartalmazza-e a GenAI C++ fejlesztoi headereket
- A helyi AI Playground bundle pontosan tartalmazza-e az `openvino_genai.lib` import library-t
- Kell-e a Windows-os in-process szerverhez tovabbi HTTP kiszolgalasi komponens, vagy a meglvo
  WinHTTP/sajat reteg elegendo
