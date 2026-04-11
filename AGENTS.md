# Masodik Agent Guide

Last updated: 2026-04-07

## Cel

Ez a repository Spec-Kit alapuan szervezett, helyi OpenVINO GenAI serving projekt. Az agentek
elso feladata mindig a `specs/` alatti aktualis feature-artifactok, majd az azokhoz tartozo kod,
teszt, smoke es benchmark allapot ellenorzese.

## Source of Truth

Prioritas sorrend:

1. `specs/<feature>/spec.md`
2. `specs/<feature>/plan.md`
3. `specs/<feature>/tasks.md`
4. a megfelelo contractok, quickstartok es data-model dokumentumok
5. az aktualis konfiguraciofajlok es a tenyleges kod

Ha dokumentacios elteres van, a dontest igy kell meghozni:

- feature-scope es elfogadas: `spec.md`
- implementacios sorrend es workflow: `tasks.md`
- aktualis futasi parameter: `config/profiles/*.json`
- tenyleges viselkedes: `src/`, `include/`, `tests/`

## Aktiv Feature-k

### `001-openvino-gpu-serving`

Lezart nagy feature. Lefedi:

- OpenAI-kompatibilis local chat servinget
- non-stream + SSE stream viselkedest
- tool-calling szerzodest
- request audit loggingot
- health/models/chat endpointokat

Fo artifactok:

- `specs/001-openvino-gpu-serving/spec.md`
- `specs/001-openvino-gpu-serving/tasks.md`
- `specs/001-openvino-gpu-serving/contracts/service-contract.md`

### `002-manual-model-split`

Lezart nagy feature. Lefedi:

- explicit ket-stage-es manual split servinget
- generated split artifact workflowt
- compiled stage-1 -> stage-2 infer utat
- 32k/128k validacios es proof workflowkat
- stage-szintu health, diagnostics es benchmark exportot

Fo artifactok:

- `specs/002-manual-model-split/spec.md`
- `specs/002-manual-model-split/tasks.md`
- `specs/002-manual-model-split/contracts/split-artifact-manifest.md`
- `specs/002-manual-model-split/contracts/split-health-contract.md`

## Jelenlegi repo-allapot

- A `tasks.md` listak alapjan a ket nagy feature feladatai elvegzett allapotban vannak.
- A jelenlegi gyakorlati fokusz a manual split serving vegallapot.
- Az aktiv profil a repo szerint: `config/profiles/b580-manual-split-cut23-tail-32k-u8.json`
- Dokumentacios konfliktus eseten az aktualis profilfajl az elsodleges a port, device es budget
  mezokhoz.

## Fontos kodteruletek

```text
include/masodik/
src/
tests/unit/
tests/integration/
tests/smoke/
benchmarks/
config/profiles/
docs/
specs/
tools/scripts/
```

Kulcsmodulok:

- API szerzodes: `include/masodik/api/`, `src/api/`
- HTTP/OpenAI route-ok: `include/masodik/server/`, `src/server/`
- runtime es health: `include/masodik/runtime/`, `src/runtime/`
- GenAI pipeline: `include/masodik/genai/`, `src/genai/`
- split serving: `include/masodik/split/`, `src/split/`
- konfiguracio/model repository: `include/masodik/config/`, `src/config/`

## Munkamod Agenteknek

### 1. Spec-Kit first

Uj munka inditasakor:

1. azonositd az erintett feature-t a `specs/` alatt
2. olvasd el minimum a `spec.md` es `tasks.md` fajlokat
3. keresd ki az erintett contractot, quickstartot es teszteket
4. csak ezutan nyulj a kodhoz

### 2. Feature-bound munka

- Ne talalj ki uj scope-ot a `specs/` megkerulesevel.
- Ha a kert modositas nem fer bele a meglevo feature-be, uj Spec-Kit feature-t kell nyitni.
- Meglevo feature bovitesnel a vonatkozo `specs/<feature>/` artifactokat is frissitsd, ha a
  szerzodes vagy workflow valtozik.

### 3. Magyar-first dokumentacio

- Komment, docs, uzemeltetesi racionalek magyarul keszuljenek.
- Technikai azonosito-nevek maradhatnak angolul.
- Nem trivialis teljesitmeny-, memoria- vagy diagnostics donteshez irj rovid magyar racionalet.

### 4. Merheto validacio

Minden erdemi valtoztatas utan legalabb az egyik fusson:

- relevans unit/integration teszt
- relevans smoke script
- relevans benchmark vagy proof kor, ha performance/split viselkedest erint

## Alap parancsok

Konfiguralas:

```powershell
.\tools\scripts\configure-local.ps1
```

Build + natív split tesztkor:

```powershell
.\tools\scripts\build-and-test.ps1
```

Quickstart validacio:

```powershell
.\tools\scripts\validate-quickstart.ps1
```

Teljes smoke suite:

```powershell
.\tools\scripts\run-smoke-suite.ps1
```

Benchmark export:

```powershell
.\tools\profiling\export-benchmark-results.ps1
```

Manual split artifact generalas:

```powershell
.\tools\scripts\generate-manual-split-artifact.ps1 `
  -OutputRoot .\build\generated-manual-split-cut23-tail-32k-u8 `
  -Profile config\profiles\b580-manual-split-cut23-tail-32k-u8.json
```

## Valtoztatasi szabalyok

- Ne regresszald a non-stream, stream, tool-calling vagy request-audit szerzodest.
- Ne engedj csendes CPU fallbacket split profilban.
- Ne tekints placeholder vagy technikai handoff-szoveget elfogadhato vegleges modellvalasznak
  valodi split artifact mellett.
- Ne modositd a smoke/benchmark proofokat ugy, hogy gyengebb elfogadasi felteteleket hasznaljanak.
- Ne irj felul user altal hagyott, nem hozzad tartozo valtozasokat.

## Tipikus ellenorzesi sorrend

### Serving/API modositasnal

1. erintett `specs/001-...` artifactok
2. `src/api/`, `src/server/`, `src/genai/`
3. unit + integration tesztek
4. smoke: chat, stream, tool-calling, request-audit

### Manual split modositasnal

1. erintett `specs/002-...` artifactok
2. `src/split/`, `src/genai/`, `src/runtime/`, `config/profiles/`
3. unit + integration tesztek
4. smoke: startup, compiled 128k, diagnostics, hardware proof

## Megjegyzes a jelenlegi workspace-rol

- A worktree lehet dirty. Mielott szerkesztesz, nezd meg a `git status` allapotot.
- A `.specify/` es egyes forrasfajlok alatt mar vannak nem commitolt valtozasok; ezekhez
  alkalmazkodni kell, nem szabad oket automatikusan visszavonni.

