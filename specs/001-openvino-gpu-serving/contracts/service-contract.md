# Service Contract: Helyi hibrid AI kiszolgalas

## Cel

Ez a dokumentum a v1 kulso es uzemeltetesi szerzodeseit rogziti az in-process, OpenAI-kompatibilis
helyi szolgaltatashoz.

## 1. Runtime dependency contract

Az in-process runtime csak akkor tekintheto ervenyesnek, ha a kovetkezo elemek elerhetok:

- helyi MSVC toolchain
- helyi OpenVINO CMake konyvtar
- helyi OpenVINO runtime DLL-ek
- helyi `openvino_genai.dll`
- a mar helyben elerheto modellrepo

Ha barmelyik hianyzik, a szolgaltatas **MUST** `failed` allapotot jelezni csendes degradacio nelkul.

## 2. Health contract

### Primary endpoint

- **Method**: `GET`
- **Path**: `/health`
- **Success**: HTTP 200 es egyertelmu `ready` allapot
- **Failure**: nem 200 vagy ertelmezheto hibavalasz

### Health semantics

- `starting`: a szolgaltatas indul, de meg nem uzemkesz
- `ready`: a modell es a szolgaltatas hasznalhato
- `failed`: konfiguracios, modellbetoltesi vagy runtime hiba tortent
- `degraded`: a kivant hybrid profil nem igazolhato vagy korlatozott

## 3. Chat inference contract

### Primary endpoint

- **Method**: `POST`
- **Path**: `/v1/chat/completions`
- **Authentication**: nincs alkalmazasszintu hitelesites a v1-ben

### Request expectations

- A kerés **MUST** tartalmazzon modell-azonositot es chat uzeneteket.
- A kerés **MAY** hosszu timeout-budgettel fusson.
- A szolgaltatas **MAY** sorba allitani a kerest nem interaktiv workloadsnal.
- A keres **MAY** tartalmazzon `tools` tombot es `tool_choice` mezot OpenAI-kompatibilis
  szerkezetben.
- A szolgaltatas **MUST** ervenyesiteni, hogy a tool definiciok strukturaltak es a tamogatott
  schema-alaknak megfelelnek.
- A szolgaltatas **MUST** request audit logginggal lathatova tenni, hogy a keres tartalmazott-e
  `tools` mezot, hany tool definicio erkezett, es milyen `tool_choice` alakban jott be.

### Response expectations

- Siker eseten a valasz **MUST** alkalmas legyen kliens oldali tovabbfeldolgozasra.
- Hiba eseten a valasz **MUST** ertelmezhetoen jelezni, hogy input-, kapacitas-, konfiguracios
  vagy modellbetoltesi problema tortent.
- Ha a modell tool hivas mellett dont, a nem streamelt valasz **MUST** strukturalt `tool_calls`
  tombot adni es a `finish_reason` mezoben ezt egyertelmuen jelezni.
- A `tool_choice` **MUST** tamogatnia a `none`, `auto`, `required` string alakokat, valamint az
  explicit `{"type":"function","function":{"name":"..."}}` formát.
- Ha `tool_choice` explicit function nevet ker, a keres **MUST** hibaval elbukni, ha a nev nem
  egyezik egyetlen meghirdetett tool definicioval sem.
- A szolgaltatas **MUST NOT** hajtson vegre szerveroldalon toolt; csak a kliens altal tovabb
  feldolgozhato strukturalt tool call payloadot adhat vissza.

### Request audit expectations

- A request audit **MUST** kulonboztetni legalabb a kovetkezo eseteket:
  - nincs `tools` mező
  - van `tools`, de `tool_choice = none`
  - van `tools`, `tool_choice = auto`
  - van `tools`, `tool_choice = required`
  - van `tools`, explicit named function `tool_choice`
- A request audit **MUST** a szerver dontesi agat is rogizteni, hogy a keres content-only vagy
  tool-calling feldolgozasra kerult-e.
- A request audit **MUST NOT** valjon korlatlan raw promptdump alapertelmezesse; az operacios
  logolasnak strukturalt, korlatozott es hibakeresesre alkalmas alakot kell kovetnie.
- A request audit `decision_path` mezonek legalabb a `content_only`, `tool_call_auto`,
  `tool_call_required` es `tool_call_named` ertekeket kell tudnia megkulonboztetni.
- A request audit `validation_outcome` mezonek legalabb a `accepted`, `parser_error`,
  `tool_schema_error`, `model_selection_error` es `runtime_readiness_error` osztalyokat kell tudnia
  felszinre hozni.
- A chat valaszok **SHOULD** request-szintu korrelacios fejlécet adniuk, hogy a kliens es a log
  ugyanahhoz a kereshez kotheto maradjon.

### Streaming response expectations

- Ha a kliens `stream: true` ertekkel kuldi a chat kerest, a valasz **MUST** `text/event-stream`
  content type-pal indulni.
- A streamelt valasz **MUST** `data:` prefixu SSE esemenyeket hasznalni.
- A sikeres stream **MUST** egy egyertelmu `data: [DONE]` lezaro esemenyt kuldeni.
- A stream megnyitasa elotti hibak **MUST** normal JSON hibavalaszkent visszaterni megfelelo HTTP
  statuskoddal.
- A mar elindult stream kozbeni terminalis hibak **MUST** SSE formaban lathatoak maradni, es a
  streamet egyertelmuen le kell zarniuk.
- A kliens megszakitasa vagy a kapcsolat bontasa **MUST NOT** hagyjon csendes, korlatlanul futo
  szerveroldali varakozast maga utan.
- A kliensabort utan a kovetkezo keresesek **MUST** tovabbra is kiszolgalhatoak maradjanak; a
  request lease es az aktiv stream szamlalo nem ragadhat bent.
- A generatorhibak **MUST** legfeljebb egyszer noveljek a stream failure diagnosztikai szamlalot
  ugyanazon streamre, hogy az uzemeltetesi metrika ne duplazzon.
- Ha a modell streaming kozben tool hivas mellett dont, a stream **MUST** OpenAI-kompatibilis
  delta esemenyekben tovabbitani a `tool_calls` reszleteit.
- A streamelt tool callingnak **MUST** megoriznie a tool call indexet, a function nevet es az
  argumentumok fokozatosan epulo tartalmat, hogy a kliens ossze tudja allitani a vegleges hivast.
- A streamelt tool-call delta objektumok **MUST** az OpenAI-szeru `choices[].delta.tool_calls[]`
  alakot kovetniuk, ahol minden delta legalabb az `index` mezot tartalmazza, es szukseg szerint
  `id`, `type`, `function.name`, `function.arguments` reszmezoket is.
- A streamelt tool call **MUST NOT** vesszen el ures content blokk vagy teves `stop` finish reason
  miatt ott, ahol a modell valojaban tool hivast valasztott.
- A tool-calling terminalis stream **MUST** `finish_reason = "tool_calls"` ertekkel zarhato, ha a
  stream vege egy strukturalt tool hivast jelent.

## 4. Model discovery contract

- **Method**: `GET`
- **Path**: `/v1/models`
- **Purpose**: a helyi aktiv modellazonosito felderitese

## 5. Bind and network contract

- A bind-cim konfiguralhato.
- A v1 celkornyezet helyi vagy helyi halozati hasznalat.
- Nyilvanos internetes kitettség nem resze a tervezett szerzodesnek.

## 6. Queue and timeout contract

- A rendszer **MUST** tamogatni a hosszu varakozasi sort agentikus, nem interaktiv futtatasra.
- A rendszer **MUST** allapotjelzessel lathatova tenni, ha a keres queue-ban van.
- A rendszer **MUST NOT** csendes elakadassal tartani a kerest.

## 7. Hybrid acceleration contract

- A szolgaltatas sikerkriteriuma nem pusztan az, hogy a modell elinduljon, hanem az is, hogy a
  kivant hybrid gyorsitasi profil atlathato modon ervenyesitheto vagy elutasithato legyen.
- Ha a kivant hybrid profil nem tarthato, a rendszer **MUST NOT** csendes lassu vegallapotba
  valtani.

## 8. Performance and compatibility baseline contract

- A szolgaltatas **SHOULD** kulon benchmarkolhato belepesi pontot adni az elso streamelt tool-call delta
  meresere.
- A non-stream es streaming tool-calling kompatibilitas **MUST** regresszios smoke-forgatokonyvvel
  vedett legyen.
- A tool-calling parity baseline **MUST** megkulonboztetni a content-only chat es a strukturalt
  `tool_calls` utakat, hogy egyik se regresszaljon a masik javitasaval.
