# Implementation Plan: Helyi hibrid AI kiszolgalas

**Branch**: `001-openvino-gpu-serving` | **Date**: 2026-04-04 | **Spec**: `C:\dev\copilot\masodik\specs\001-openvino-gpu-serving\spec.md`
**Input**: Feature specification from `C:\dev\copilot\masodik\specs\001-openvino-gpu-serving\spec.md`

**Note**: Ez a terv a mar meglevo streaminges es tool-calling kepes helyi OpenAI-kompatibilis szervert
finomitja tovabb ugy, hogy a request logging explicit lathatova tegye: OpenClaw kuldott-e `tools`
mezot, milyen `tool_choice` ertekkel, es a keres melyik feldolgozasi agra kerult.

## Summary

A megvalositas celja egy helyi, OpenAI-kompatibilis AI szolgaltatas tovabbfejlesztese, amely a mar
letoltott `OpenVINO---gpt-oss-20b-int4-ov` modellre epul, ugyanazon a fejlesztoi gepen fut, es a
hybrid gyorsitasi profilt kozvetlen modellbetoltessel probalja ervenyesiteni. A szolgaltatas nem
kulso modellserver-folyamatot indit, hanem a modell futtatasi reteget a sajat C++ alkalmazasba
integralja, igy az eszkozkiosztas, a diagnosztika, az inditasi hibakezeles, az SSE streaming, a
tool calling es most mar a request audit logging is egy kezben marad.

A kovetkezo tervezesi kor fo nyeresege, hogy a mar mukodo chat + SSE + tool-calling ut melle kulon
request logging reteget kapunk. Ez lehetove teszi, hogy OpenClaw-szeru kliensintegracioknal utolag
is bizonyithato legyen: a kliens tenylegesen kuldott-e `tools` tombot, milyen `tool_choice`-szal,
es a szerver miert maradt content-only vagy miert valtott tool-calling agra.

## Technical Context

**Language/Version**: C++23 a helyi MSVC 14.50.35717 toolsettel  
**Primary Dependencies**: OpenVINO 2026.1.0 helyi runtime, OpenVINO GenAI C++ pipeline, WinHTTP vagy ezzel egyenerteku helyi HTTP kiszolgalasi es kliens reteg, helyi JSON konfiguracio es diagnosztikai segedretegek  
**Storage**: helyi fajlrendszer; meglevo modellrepo `C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\models\LLM\openvino\OpenVINO---gpt-oss-20b-int4-ov`; konfiguracios fajlok, cache, benchmark eredmenyek es logok a projektben  
**Testing**: CTest + sajat C++ teszt executable-k, PowerShell smoke tesztek, tool-calling regresszios integracios tesztek, cold-start / first-token / throughput / stability benchmark futtatok  
**Target Platform**: Windows workstation a celgepen: Intel Core 7 Ultra 265K, 64 GB RAM, Intel B580 12 GB GPU  
**Project Type**: nativ C++ helyi AI szolgaltatas in-process modellbetoltessel  
**Performance Goals**: cold start <= 5 perc; elso valasz reprodukalhato baseline szerint; hybrid profil inditasanak es hibakepenek atlathatosaga; stabil kiszolgalas legalabb 5 egyideju keres mellett hosszu timeoutablakkal; tool callingnal a tool-call emisszio nem veszhet el streaming alatt; request logging ne okozzon erdemi stabilitasi regressziot es egyertelmuen bizonyitsa a `tools` / `tool_choice` bemenetet  
**Constraints**: kotelezo a helyi MSVC toolchain hasznalata; kotelezo a mar letoltott helyi modell ujrafelhasznalasa uj letoltes nelkul; kotelezo az AI Playground altal telepitett OpenVINO 2026.1.0 runtime figyelembe vetele; a kulso modelserver runtime path nem lehet a cel-MVP alapja; a hybrid gyorsitas sikertelensege nem fedheto el csendes altalanos degradacioval; a tool calling szerveroldalon nem hajthat vegre klienstol kapott eszkozmuveletet, csak strukturalt tool call payloadot adhat vissza; request logging nem logolhat korlatlanul teljes promptdumpokat kontroll nelkul  
**Scale/Scope**: egy gep, egy elsoleges modell, helyi OpenAI-kompatibilis chat szolgaltatas, health/model-lista tamogatas, merheto startup- es terhelesi viselkedes, teljes tool calling keret egy kliensoldali orchestrator szamara, valamint utolagos request audit OpenClaw-kompatibilis hibakereseshez

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Modern C++ and MSVC First**: PASS - a terv tovabbra is C++23 + helyi MSVC Build Tools toolchainre epul; a request logging bovites sem igenyel idegen runtime-ot.
- **Hardware-Tuned OpenVINO Serving**: PASS - a celgep konkret hardverprofilja, a hybrid eszkozkiosztas, a helyi modellcache es most mar a request audit is a gepre szabott kiszolgalasi modell resze.
- **Measurable Performance and Stability Gates**: PASS - a cold start, az egyideju keresek, a first-response baseline, a stream stabilitas, a tool-call emisszio es a request logging lathatosaga mind reprodukalhato kimenetkent szerepel.
- **Test, Benchmark, and Regression Discipline**: PASS - a terv CTest, smoke teszt, tool-calling regresszio es benchmark futtatok mellett request-audit regressziokkal is szamol.
- **Hungarian-First Maintainability**: PASS - a diagnosztikai mezok, uzemeltetesi racionalek es nem trivialis megjegyzesek magyarul keszulnek.

**Post-Design Re-check**: PASS - a request logging bovites erositi a diagnosztikai atlathatosagot anelkul, hogy az alkotmanyi elveket sertene.

## Project Structure

### Documentation (this feature)

```text
specs/001-openvino-gpu-serving/
+-- plan.md
+-- research.md
+-- data-model.md
+-- quickstart.md
+-- contracts/
|   +-- service-contract.md
+-- tasks.md
```

### Source Code (repository root)

```text
include/
+-- masodik/
    +-- api/
    +-- config/
    +-- diagnostics/
    +-- genai/
    +-- runtime/
    +-- server/

src/
+-- main/
+-- api/
+-- config/
+-- diagnostics/
+-- genai/
+-- runtime/
+-- server/

tests/
+-- unit/
+-- integration/
+-- smoke/
+-- fixtures/

benchmarks/
+-- cold-start/
+-- steady-state/
+-- queue-soak/

tools/
+-- discovery/
+-- profiling/
+-- scripts/

config/
+-- profiles/
```

**Structure Decision**: A repository tovabbra is nativ C++ szolgaltatas marad `genai/`, `server/`,
`diagnostics/` es `runtime/` modulokra szervezve. A request logging a `server/` es `diagnostics/`
retegben kap helyet, mikozben a kulso OpenAI contract es a tool-calling DTO-k az `api/` modulban
maradnak.

## Migration Strategy

1. **Runtime pivot**: A helyi modellcsomag tovabbra is kozvetlenul toltodik be, kulso szerverfolyamat nelkul.
2. **Service pivot**: A chat, model-lista, health es tool-calling feluletek melle request audit logging kerul.
3. **Streaming pivot**: A jelenlegi content-only es tool-call SSE delta logika melle request-szintu bemeneti diagnosztika is kell.
4. **Compatibility pivot**: Az OpenClaw-szeru klienseknel utolag bizonyithato kell legyen, hogy a bemenet tartalmazott-e `tools` es `tool_choice` mezot.
5. **Observability pivot**: A diagnosztikanak latnia kell a tool-call stream hibakat, az elveszett chunkokat es a request szerzodes aktiv voltat.

## Phase Outline

### Phase A - Request audit contract

- request log schema `stream`, `tools_present`, `tool_count`, `tool_choice_kind`, `tool_choice_name`, `decision_path`
- adatminimalizalasi szabalyok a prompt- es arguments-logolasra
- OpenClaw hibakeresesi forgatokonyvekhez olvashato audit kimenet

### Phase B - Server es diagnostics wiring

- request logging primitivek a route retegben
- request id + audit bejegyzes osszerendeles
- kulon content-only es tool-calling feldolgozasi ag jelzese

### Phase C - Regression es operacios validacio

- unit/integration/smoke regressziok a request audit mezokre
- OpenClaw-szeru manualis vizsgalatok dokumentalasa
- teljesitmeny es logmeret trade-off rogzites

## Immediate Planning Consequences

- A jelenlegi runtime log csak startup-szintu informaciot mutat; ezt request-szintu audit loggal kell kiegesziteni.
- A jelenlegi tool-calling heurisztika csak a feldolgozasi dontest hozza meg; ezt request loggingnak is tukroznie kell.
- A request auditnak kulon latnia kell, hogy a kliens egyaltalan kuldott-e `tools` tombot vagy `tool_choice` mezot, mert ez OpenClaw alatt kulcsfontossagu hibakeresesi pont.
- Az audit logging nem valhat teljes prompt dumpolassa; strukturalt, korlatozott es uzemeltetesi celu mezokre kell szoritkoznia.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Nincs alkotmanysertes | N/A | N/A |
