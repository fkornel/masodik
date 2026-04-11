# Legacy OVMS Baseline Decommission Notes

## Mi maradt meg

Az `ovms/` és a kapcsolódó discovery/launcher baseline egy része még jelen van fordítási és történeti kompatibilitás miatt.

## Mi már nem cél

- külső OVMS folyamat indítása mint elsődleges runtime út
- Continuous Batching mint alap serving motor
- hybrid GPU+CPU profil érvényesítése OVMS-en keresztül

## Mi a jelenlegi célállapot

- in-process OpenVINO GenAI pipeline
- saját HTTP / OpenAI-kompatibilis szerverréteg
- explicit startup, degraded és failed állapotok
- mérhető összehasonlítás pure GPU és HETERO profilok között

## Takarítási irány

Ha a későbbi körökben már nincs szükség kompatibilitási hídra, az OVMS-specifikus kód külön takarítási körben kivezethető.
