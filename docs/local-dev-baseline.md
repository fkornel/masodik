# Helyi fejlesztoi baseline

## Cel

Ez a dokumentum a jelenlegi helyi gepen elerheto, in-process OpenVINO/GenAI fejleszteshez
szukseges alap komponensek allapotat rogziti.

## Rendelkezesre allo helyi komponensek

- MSVC Build Tools es CMake/CTest/Ninja elerheto a Visual Studio Build Tools telepitesbol
- A helyi OpenVINO CMake konyvtar elerheto az AI Playground telepitesebol
- A mar letoltott `OpenVINO---gpt-oss-20b-int4-ov` modellrepo elerheto
- `openvino_genai.dll` es `openvino_genai_c.dll` jelen van a helyi OpenVINO bundle-ben
- A projektbe bemasolt `C:\dev\copilot\masodik\openvino.genai-2026.1.0.0` forras tartalmazza a
  GenAI C++ API headereket es a Windows/MSVC CMake buildleirasokat
- A projektbe bemasolt `C:\dev\copilot\masodik\openvino_genai_windows_2026.1.0.0rc2_x86_64`
  Windows bundle tartalmazza a mukodo `OpenVINOConfig.cmake`, `OpenVINOGenAIConfig.cmake`,
  `openvino_genai.lib` es a futashoz szukseges DLL-keszletet

## Kulon ellenorizett fejlesztoi artefaktumok

- A helyi audit a jelenlegi AI Playground bundle-ben megtalalta az `openvino_genai.dll` runtime
  komponenst
- A helyi audit nem talalt `openvino_genai.lib` import library-t a szokasos AI Playground/Intel
  telepitesi gyokerekben
- A helyi audit nem talalt egyertelmuen hasznalhato GenAI C++ headerkeszletet sem
- Emiatt az in-process HTTP/server reteg mar fejlesztheto, de a tenyleges C++ GenAI linkeleshez
  vagy pontosabb helyi forras, vagy kulon fejlesztoi csomag kellhet
- A bemasolt GenAI source configure-probaja azt mutatta, hogy a forras `OpenVINO 2026.1.0`
  kompatibilitast ker, mikozben a helyi AI Playground OpenVINO CMake package verzioja
  `2025.4.1`
- A kulon kicsomagolt Windows GenAI bundle viszont sajat OpenVINO 2026.1.0 runtime/cmake
  csomagot ad, es ezzel a `masodik-launcher --validate-and-stop` mar sikeresen `ready`
  allapotig jut

## Kovetkezmeny

Jelen allas szerint uj modellletoltes nem szukseges, es a T087-hez szukseges GenAI C++ runtime mar
elerheto a kulon Windows bundle-bol. A fo nyitott pont innentol inkabb a hybrid profil
teljesitmenye es stabilitasa, nem a C++ GenAI linkeles.
