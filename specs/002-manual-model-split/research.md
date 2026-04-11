# Research: Manual Model Split Serving

## Decision 1: A HETERO `PIPELINE_PARALLEL` tamogatas nem lesz a feature fo megoldasa

**Decision**: A feature elsodleges iranya explicit, kezzel felugyelt ket-stage serving lesz, nem pedig a monolitikus modellre bizott HETERO `MODEL_DISTRIBUTION_POLICY=PIPELINE_PARALLEL`.

**Rationale**:
- A dokumentacio es a runtime property-k alapjan a `PIPELINE_PARALLEL` mint hint valoban letezik es atadhato.
- A celgepen vegzett korabbi futasokban azonban a monolitikus `HETERO:GPU.1,GPU.0` utvonal nem adott bizonyithato, stabil ket-stage kiosztast: a dGPU foglalta a modellfo reszt, az iGPU nem mutatott tartos modellterhelest, a 128k soft-forcing pedig tulzott RAM-terhelest okozott.
- A jelenlegi OpenVINO GenAI forrasban a HETERO utvonal es a scheduler/paged-attention interakcio tovabbi kockazatot jelent: ha a `scheduler_config` nincs kontroll alatt, a continuous batching default `max_num_batched_tokens = SIZE_MAX` iranyba mehet, ami a korabbi 39 GB+ RAM jelenseggel osszhangban van.
- A feature celja nem elvi tamogatas, hanem determinisztikus, reprodukalhato stage-megosztas.

**Alternatives considered**:
- Csak a HETERO `PIPELINE_PARALLEL` hasznalata a meglvo monolitikus modellen.
- HETERO + 128k kontextus mint "kenyszerito" eszkoz.
- HETERO + kulon scheduler/property-k tovabbi hangolasa.

## Decision 2: Ket kulon split artifact lesz a szolgaltatas bemenete

**Decision**: A szolgaltatas nem egyetlen modellkonyvtarat, hanem egy ketreszes split artifact-keszletet fogad, amely stageenkent kulon loader-informaciot es kompatibilitasi metadata-t tartalmaz.

**Rationale**:
- A split artifact teszi determinisztikussa a reteghatart, a stageek input/output szerzodeset es a validalhatosagot.
- A startup csak igy tudja megelozni, hogy a rendszer reszlegesen betoltott vagy inkompatibilis allapotban ready legyen.
- A kesobbi ujravagasok vagy alternative stage-hatarok ugyanazon szerzodes szerint kezelhetok.

**Alternatives considered**:
- A stage-hatart csak profilban rogzitett, kulso metadata nelkul.
- A splitet kizarolag futasi konfiguraciobol levezetni.

## Decision 3: Keresenkenti explicit hidden-state handoff szukseges

**Decision**: Az elso stage kimenete keresenkent izolalt handoff-objektumba kerul, amelyet a masodik stage fogyaszt el.

**Rationale**:
- Ez megakadalyozza a parhuzamos keresek allapotkeveredeset.
- A handoff pont kulon diagnosztizalhato lesz.
- A keresenkenti ownership egyertelmuen kezeli a siker, hiba, timeout es megszakitas allapotokat.

**Alternatives considered**:
- Globalis, ujrafelhasznalt buffer.
- Olyan implicit atadas, amely csak az infer-request-ek belso allapotara tamaszkodik.

## Decision 4: A kezdeti topology dGPU-elso, iGPU-masodik stage lesz

**Decision**: A tervezett elso topology: Stage 1 -> GPU.1 (B580), Stage 2 -> GPU.0 (iGPU).

**Rationale**:
- A dGPU nagyobb dedikalt VRAM-ja jobb hely az elso, jellemzoen nehezebb stage-nek.
- Az iGPU a masodik stage-ben nagy shared memory teret hasznalhat a terjeszkedo kontextushoz.
- Ez a topology illeszkedik a felhasznaloi strategiai celhoz: a CPU szabadon hagyasa mellett ket GPU bevonasa.

**Alternatives considered**:
- iGPU-elso, dGPU-masodik topology.
- dGPU + CPU hibrid topology mint elsodleges cel.

## Decision 5: A split profil explicit fallback-tiltassal indul

**Decision**: Ha a ket stage kozul barmelyik csak CPU-n vagy ugyanazon gyorsiton lenne futtathato, a split profil startupja hibaval leall.

**Rationale**:
- A feature uzleti celja a valodi ket-GPU-s serving, nem a csendes visszaeses.
- Az eddigi tapasztalatok szerint a "ready" jelzes onmagaban nem eleg, ha valojaban nem a vart topology fut.
- Ez vedi a benchmarkokat es az uzemeltetesi ertelmezhetoseget.

**Alternatives considered**:
- Csendes fallback egyetlen gyorsitora.
- CPU-ra valo reszleges visszaeses warninggal.

## Decision 6: A 128k kontextus tamogatasa szakaszosan, memoria-budgettel tortenik

**Decision**: A design ugy kezeli a 128k kontextust, mint kulon validalando serving-modot, amelynek stageenkenti memoria-budgetje, puffere es overload-viselkedese kifejezetten merendo es ellenorizendo.

**Rationale**:
- A korabbi 128k kiserlet a monolitikus HETERO uton nem adott stabil eredmenyt.
- A split serving erteke pont az, hogy a memoria-terheles iranyitottan eloszthato legyen.
- A tervnek mar most ki kell zarnia a teljes gepet megbenito memoriafelfutast.

**Alternatives considered**:
- 128k tamogatast mar a legelso MVP-ben altalanos esetkent vallalni.
- Konkret memoria-korlatok nelkul csak funkcionalis tamogatast tervezni.

## Decision 7: Stage-szintu health es diagnosztika uj kulcskovetelmeny

**Decision**: A split serving nem elegedhet meg egyetlen globalis ready flaggel; stage- es handoff-szintu allapot, hibaok es memoria-informacio kell.

**Rationale**:
- A ket-stage futasnal mas a startup-hiba, mas az atadasi hiba es mas az uzem kozbeni eroforras-kimerules.
- Az operator csak igy tudja gyorsan eldonteni, hogy artifact, device, memory vagy handoff problema tortent.

**Alternatives considered**:
- Csak globalis `/health`.
- Csak naplozas, kulon stage-statusz nelkul.

## Decision 8: Az interface szerzodesek ket uj feluletet fednek le

**Decision**: A Phase 1 contractok a split artifact manifestet es a split health/diagnosztikai payloadot rogzitik.

**Rationale**:
- Ez a ket uj kulso-belso hatarpont vezeti be a feature valodi uzemeltetesi szerzodeset.
- A chat endpoint maga valtozatlan kell maradjon, ezert a valtozasok a konfiguracios es health szerzodesben jelennek meg.

**Alternatives considered**:
- Kizarolag plan-szovegben hagyni a szerzodeseket.
- Chat payload szinten uj protokollt bevezetni.

## Decision 9: A kezdeti idealis vagascel a `layers.17 -> layers.18` hatar

**Decision**: A gpt-oss-20b-int4-ov elso konkret split-celja a `__module.model.layers.17.*` es `__module.model.layers.18.*` kozotti vagas, amely gyakorlati tervezesi szinten kb. 18 blokkot hagy a B580 dGPU-n, es kb. 26 blokkot az iGPU-n.

**Rationale**:
- Ez illeszkedik a helyi gepspecifikus memoriacelhoz: a B580 12 GB VRAM-jabol a Windows/DWM miatt kb. 1.5-2.0 GB-t eleve fenn kell tartani.
- A maradek fizikai VRAM-ba igy realisan befer kb. 5.0-5.5 GB stage-1 suly, kb. 3.0-3.5 GB stage-1 KV/scratch tartalek es kb. 1.0 GB driver-overhead.
- A maradek retegek iGPU oldalon jobban parosithatok a 15-20 GB koruli Stage-2 KV ownershippel es a 128k contextcel.

**Alternatives considered**:
- Korabbi vagas a `layers.15 -> layers.16` hatarnal, nagyobb iGPU-terhelessel.
- Kesobbi vagas a `layers.19 -> layers.20` hatarnal, nagyobb B580 terhelessel.

## Decision 10: A 128k context ownership aszimmetrikus lesz

**Decision**: A 128k context nem egyenlo aranyban oszlik el; a Stage-2 iGPU oldala viszi a fo KV-cache terhet, mig a Stage-1 B580 oldal elsosorban a gyors prefill/decode elejet tartja helyben.

**Rationale**:
- A B580 dGPU shared memoryra valo kilapozasa lassu es instabil uzemi cel, foleg monitoros desktop mellett.
- Az iGPU a rendszermemoriahoz kozelebb es nagyobb mozgastartomannyal tudja hordozni a Stage-2 KV-cache-et.
- A feature vegere az kell bizonyitani, hogy a Stage-1 nincs rakenyszeritve a dGPU shared memoryra a normal uzemhez.

**Alternatives considered**:
- Egyenlo Stage-1 / Stage-2 KV felosztas.
- A teljes nagy context kizarlag Stage-1 oldali ownershipje.

## Decision 11: US4 — Optimalis reteg-elosztás a 128k deep split-hez

**Decision**: 18 reteg a dGPU-n (0-17), 6 reteg + tail az iGPU-n (18-23+tail). Ez uj profil (`b580-deep-split-128k`), a meglevo 32k profil valtoztalan marad.

**Rationale**: A B580 dGPU 12GB VRAM-mal rendelkezik. 128k context-nel a KV cache retegenkent 134MB (u8). A modell suly retegenkent ~420MB (INT4). 18 reteg: 18 * 554 MB + 1 GB overhead = ~11.0 GB, ami ~1 GB tartalekkal belfer a 12 GB-ba. A 18 reteg maximalizalja a dGPU kihasznaltsagat — minnel tobb reteg a gyorsabb dGPU-n, annal jobb a teljesitmeny.

**Alternatives considered**:
- 16 reteg dGPU: Biztonsagos de 2 GB VRAM kihasználatlan.
- 19 reteg dGPU: 11.53 GB, csak ~470 MB tartalek — kockázatos.
- 20 reteg dGPU: Nincs tartalek, OOM kockazat.

## Decision 12: US4 — iGPU KV cache a rendszermemoriaban

**Decision**: Az iGPU 6 reteg KV cache-e a 64GB rendszermemoriából allokálodik (shared memory), nem dedikalt VRAM.

**Rationale**: Az Intel iGPU megosztott memoriát hasznal. A 6 reteg KV cache ~0.8 GB + modell suly ~2.5 GB = ~3.3 GB, ami a 64 GB RAM-bol boven elfer. Ez az architekturalis kulcs a 128k context-hez.

## Decision 13: US4 — Handoff mechanizmus megfelelo 128k-hoz

**Decision**: A jelenlegi chunked handoff (768 token/chunk) megfelelo, nincs szukseg streaming handoff-ra.

**Rationale**: Decode fazisban a handoff 11.5 KB/token. Prefill-nel 8.85 MB/chunk. Egyik sem memoria-kritikus. A korabbi ~136 GB robbanás a chunking nelkuli teljes szekvencia egyszeri atadása volt.

## Decision 14: A split artifact generalas a feature kotelezo resze

**Decision**: A monolitikus AI Playground modell utolagos splitje nem opcionis elokeszites, hanem a feature kotelezo deliverable-je.

**Rationale**:
- Jelenleg nincs a gepen hasznalhato `split-manifest.json`-os artifact root, igy a runtime nem tud hiteles manual split servinget bizonyitani.
- A feature uzleti erteke megszunik, ha csak split-aware loader es diagnosztika marad, de nincs tenyleges stage-1/stage-2 artifact.
- A lezaro proof kor csak generated split artifacttal futtathato oszinten.

**Alternatives considered**:
- Kulso kezi artifact gyartas es dokumentalatlan bemasolasa.
- A split artifact feature-scope-bol valo kivetele es kulso prerequisite-kent kezelese.
