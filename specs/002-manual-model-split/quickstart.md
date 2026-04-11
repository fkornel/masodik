# Quickstart: Manual Model Split Serving

## Cel

Ez a quickstart a tervezett split serving MVP uzemeltetesi ellenorzesenek lepesrendjet rogziti.

## Elokeszuletek

1. Legyen ervenyes split artifact-keszlet a feature szerinti manifesttel.
2. Legyen kulon split profil, amely az elso stage-et a dGPU-ra, a masodikat az iGPU-ra koti.
3. A gepen mindket gyorsito elerheto legyen.
4. A Windows Graphics Settings alatt a launcher/app legyen `High Performance` modra allitva, de a bizonyitas tovabbra is az explicit `GPU.1` / `GPU.0` runtime-hozzarendelesen alapul.
5. A B580 monitoros dGPU-headroomja miatt a Stage-1 nem futhat a fizikai VRAM-kereten tulra tervezett budgettel.

### Generated artifact workflow

1. A monolitikus AI Playground modell-rootbol generald le a split artifactot:

```powershell
.\tools\scripts\generate-manual-split-artifact.ps1 `
  -OutputRoot .\build\generated-manual-split-actual `
  -MaxContextTokens 131072 `
  -ValidatedContextTokens 131072
```

2. Ellenorizd, hogy a builder a `layers.17 -> layers.18` hatart rogziti a manifestben.
3. Ellenorizd, hogy a generated root tartalmazza a `stage-1` es `stage-2` IR-eket.
4. A generated rootot add at a launchernek `--model-root` parameterkent.

## Inditasi folyamat

1. Inditsd a szolgaltatast a split profillal es a generated artifact root-tal.
2. Ellenorizd a startup statuszt:
    - mindket stage betoltott-e,
    - a fallback tiltasa ervenyesult-e,
    - a health nem jelzi-e valamelyik stage hibat,
    - az aktiv split artifact verzio es a `request_path_mode` mezok megjelennek-e,
    - a stage warmup logban a `execution_devices` mezok tenylegesen `GPU.1` es `GPU.0` hozzarendelest mutatnak-e.
3. Fusson le egy warmup keres, amely teljesen vegigmegy mindket stage-en, es a health ezutan `request_path_mode=compiled` erteket adjon.

## Cel topologia ehhez a gephez

1. **Stage-1 / B580 dGPU**: kezdetben kb. 18 blokk, kb. 5.5 GB suly + kb. 3.5 GB KV/scratch budget, 1.5-2.0 GB Windows/DWM headroom mellett.
2. **Stage-2 / iGPU**: kezdetben kb. 26 blokk, kb. 7.0 GB suly + kb. 15-20 GB KV ownership a 128k contexthez.
3. **Tiltott uzemi allapot**: a B580 shared memoryra valo csendes kilapozas mint normal serving-strategia.

**Megjegyzes a jelenlegi valos artifactrol**: a helyi builder a geprol jelenleg `cut_after_layer=layers.17 -> layers.18`
ertekkel es `stage_1_layer_range=0-17`, `stage_2_layer_range=18-23` topologiaval general ervenyes artifactot. A
tervezett 18/26 cel tovabbra is iranyado budget-szabaly, de a lezaro proofhoz mindig az aktualisan generalt manifest az
iranyado forras.

## Funkcionalis ellenorzes

1. Kuldd be az alap chat-kerest.
2. Ellenorizd, hogy a valasz rendben megerkezik.
3. Ellenorizd, hogy a split health payload mindket stage-re `ready` allapotot mutat.
4. Ellenorizd, hogy a valasz **nem** placeholder, es a health `request_path_mode=compiled` erteket ad.
5. A generated split artifacton a valasz mar **valodi detokenizalt modellszoveg** legyen; a `Compiled split infer sikeres...` jellegu technikai handoff-osszefoglalo itt mar regresszionak szamit.
6. Ha a split artifactbol hianyzik az `openvino_tokenizer.xml` es `openvino_detokenizer.xml` par, a runtime csak tesztfixture-kompatibilis technikai fallbacket tud adni; operatori proofhoz ez nem elfogadhato.

## Hosszu kontextusu ellenorzes

1. Indits egy reprezentativ hosszu kontextusu kerest.
2. Figyeld a stage-szintu memoriahelyzetet es a handoff allapotot.
3. A teszt akkor sikeres, ha:
    - a rendszer valaszt ad,
    - nem lep unhealthy allapotba,
    - nem tortenik nem vart fallback,
    - a handoff allapot a keres utan `consumed`,
    - a memory summary stage-enkent budget/required jelzest ad,
    - a `validated_context_tokens` eleri a 131072 erteket,
    - a `memory_summary.system` mezoben `mode=compiled` es `handoff_bytes=` is latszik.
4. Low-concurrency split uzemben egy masodik egyideju keresnek szabalyos overload visszautasitast kell kapnia, nem csendes sorbaallast.

## Hibaellenorzes

1. Probalj inditast hianyos vagy inkompatibilis masodik stage-del.
2. Ellenorizd, hogy a startup nem lesz ready.
3. Ellenorizd, hogy a health es a naplo egyertelmuen jelzi az erintett stage-et.

## Diagnosztikai ellenorzes

A split serving diagnosztikajat a kovetkezo modszerekkel lehet ellenorizni:

### Health payload ellenorzese

```powershell
# Teljes split health payload lekerdezese
$health = Invoke-RestMethod http://127.0.0.1:29011/health
$health | Format-List split_mode, split_profile, stage_1_state, stage_2_state, handoff_state, request_path_mode, stage_1_device, stage_2_device, split_artifact_version, validated_context_tokens, last_failure_scope, fallback_detected, last_failure_summary
$health.memory_summary | Format-List
```

### Compiled 128k smoke

```powershell
.\tests\smoke\test_manual_split_compiled_128k.ps1 -BaseUrl http://127.0.0.1:29011
```

### Stage diagnosztika smoke teszt

```powershell
.\tests\smoke\test_manual_split_diagnostics.ps1 -BaseUrl http://127.0.0.1:29011
```

Ez a smoke teszt validalja:
- Minden split-specifikus health mezo letezik
- Ready allapotban mindket stage `ready`
- Memory summary stage-enkent budget jelzest ad
- Ket egymas utani lekerdezés konzisztens
- A `fallback_detected` es `last_failure_scope` konzisztens a ready allapottal
- A `request_path_mode` nem `placeholder`
- A compiled 128k smoke mar nem fogadja el a `Compiled split infer sikeres...` technikai valaszt a valodi split artifacton

### Log diagnosztika

A split runtime strukturalt log sorai:
- `split_stage stage_id=... state=... device=...` — stage-szintu allapotvaltozas
- `split_failure_summary scope=... stage_1=... stage_2=...` — hiba osszefoglalas
- `manual_split_runtime_failure scope=...` — runtime hiba scope-pal

```powershell
Select-String -Path build\logs\masodik-runtime.log -Pattern "split_stage|split_failure_summary|manual_split"
```

### Diagnosztikai benchmark

```powershell
$env:MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT = "C:\models\gpt-oss-20b-split"
.\build\RelWithDebInfo\masodik-benchmarks.exe benchmark_manual_split_diagnostics_overhead
```

### Ket-GPU-s proof

1. Futtasd le a dedikalt hardveres proof workflowt:

```powershell
.\tests\smoke\test_manual_split_hardware_proof.ps1 `
  -ModelRoot C:\dev\copilot\masodik\build\generated-manual-split-actual
```

2. A smoke a `tools\scripts\capture-manual-split-proof.ps1` scriptre epul, amely:
   - elinditja a split launchert,
   - kuld egy valodi compiled split kerest,
   - a launcher PID-re szurve mintavetelezi a Windows `GPU Process Memory` es `GPU Engine` szamlaloit,
   - JSON proof artifactot ment a `build\artifacts\manual-split-proof\` ala.
3. A proof kor csak akkor fogadhato el, ha:
   - a health `request_path_mode=compiled`,
   - a health nem ures `stage_1_execution_devices` es `stage_2_execution_devices` mezoket ad,
   - a proof artifact legalabb ket aktiv adapter-LUID mintat rogzit ugyanazon launcher process alatt.
4. Operatori ellenorzesnel ezek utan mar a Feladatkezeloben is ugyanennek kell latszodnia:
   - a B580 dedikalt VRAM-ja novelkedik es terhelt,
   - az iGPU shared memory oldalan is lathato aktivitas vagy memoriahasznalat jelenik meg,
   - a CPU nem valik fo inferencia-utvonalla.

## Benchmark kiindulopont

1. Cold-start meres split profillal.
2. Egy rovid warm-latency meres.
3. Egy reprezentativ long-context stabilitasi meres.
4. Egy alacsony konkurencias stage-handoff regresszioellenorzes.

Ajánlott benchmark entrypointok:

1. `benchmark_manual_split_cold_start`
2. `benchmark_manual_split_long_context`
3. `benchmark_manual_split_handoff_latency`
4. `benchmark_manual_split_stability_proof`

## Elfogadasi minimum

- A split startup kovetkezetesen vegbemegy.
- A globalis ready csak teljes ket-stage kesi allapotnal jelenik meg.
- A reprezentativ hosszu kontextusu keres nem destabilizalja a gepet.
- A diagnosztika legalabb stage-szinten megmutatja a hibateruletet.
- A low-concurrency queue policy nem enged kontrollalatlan masodik long-context kerest.
- A zaraskor mar nem eleg a startup-ready: valodi split artifact, compiled request-ut es ket-GPU-s proof kotelezo.
