# Implementation Plan: Chat Funkciok

**Branch**: `005-chat-features` | **Date**: 2026-04-12 | **Spec**: [spec.md](spec.md)
**Status**: Implemented
**Input**: Chat szemelyre szabas, beviteli mezo javitas, markdown rendereles, system prompt, chat history.

## Summary

A masodik TUI chat alkalmazas szemelyre szabasa (Kornel + Szilvabot nevek), a beviteli mezo javitasa (tobbsoros paragraph rendereles, Alt+Enter, paste, Ctrl+U, inaktiv generalas kozben), markdown rendereles a valaszokban (bold, ujsor, kodblokk), system prompt (SYSTEM_PROMPT.md), JSONL chat history, Esc megszakitas, korabbi uzenetek szerkesztese megerositessel, kontextus meret ellenorzes.

## Szponzori dontesek (rogzitett)

- Chat history tarolas: **JSONL append-only fajl** (std::ofstream std::ios::app, NTFS optimalis)
- Konfiguracio: **fajl alapu** (config/masodik.json bovites: nevek, prompt fajl, history fajl, generalasi parameterek)
- System prompt: **SYSTEM_PROMPT.md fajl** a repo gyokereben (az AGENTS.md fejlesztoi guide maradt)
- Nevek: konfiguracios fajlbol (nem kodban rogzitett)
- Generalasi parameterek: masodik.json-bol felulirjak a profil ertekeket

## Technical Context

**Language/Version**: C++23, MSVC, FTXUI v6.1.9
**Primary Dependencies**: FTXUI (TUI), meglevo masodik framework, std::filesystem, std::ofstream, std::chrono
**Storage**: JSONL append-only fajl (data/chat-history.jsonl), config/masodik.json, SYSTEM_PROMPT.md
**Testing**: Meglevo CTest + manualis TUI tesztek + data/tui-debug.log
**Target Platform**: Windows 11 + Windows Terminal
**Constraints**: NTFS optimalis fajlmuvelet, nincs SQL, nincs kulso fuggoseg

## Architektura

### Konfiguracios fajlok

```json
// config/masodik.json:
{
  "profile": "config\\profiles\\b580-tail-split-128k.json",
  "model_root": "build\\generated-manual-split-tail-128k-u8",
  "mode": "tui",
  "user_name": "Kornel",
  "agent_name": "Szilvabot",
  "agent_prompt_file": "SYSTEM_PROMPT.md",
  "chat_history_file": "data\\chat-history.jsonl",
  "max_tokens": 1024,
  "temperature": 0.6,
  "repetition_penalty": 1.15,
  "top_p": 0.85,
  "top_k": 40
}
```

```markdown
// SYSTEM_PROMPT.md — Szilvabot system prompt + stilusjegyek
# Szilvabot
Te Szilvabot vagy, egy AI ugynok. Magyarul beszelgetunk.
## Stilusjegyek
- Roviden, termeszetesen valaszolj. 1-3 mondat eleg.
- Ne hasznalj felsorolast, hacsak nem kerdik.
```

### Chat History JSONL formatum

```jsonl
{"role":"user","name":"Kornel","content":"Szia!"}
{"role":"assistant","name":"Szilvabot","content":"Szia!","thinking":"..."}
```

Minden uzenet egy sor. Append-only iras: `std::ofstream(path, std::ios::app)`.
Betoltes: soronkent JSON parse. Serult sorok: kihagyas (nem crash).

**Kiveteles eset**: uzenet szerkesztes (US7) — megerosites utan teljes ujrairas.

### Beviteli mezo

- paragraph() alapu rendereles — szotoreses sortordeles
- A kurzor vizualis: █ blokk karakter a szoveg vegen (villog)
- Max 8 sor magassag (size(HEIGHT, LESS_THAN, 8))
- 3+ soros tartalom: osszevont megjelenitest "[N sor, M kar.]"
- Alt+Enter / Shift+Enter (kitty) → uj sor
- Ctrl+U → teljes torles
- Paste: idozites-alapu detektalas (Enter 20ms-on belul karakter utan = paste ujsor)
- Generalas/browse kozben: dim, inaktiv, minden event blokkolva

### Browse navigacio (US7)

- Ures mezobol ArrowUp → browse mod (browse_index = utolso user uzenet)
- ArrowUp/Down → navigacio, chat scroll koveti (focus a kijelolt uzenetren)
- Enter → uzenet betoltese a mezobe (semmi nem torlodik), browse mod vege
- Kuldes (Enter a betoltott uzenettel) → megerosito modal ha van torlendo uzenet
- Esc → browse mod vege, visszateres a beviteli mezore
- browse kozben: input inaktiv, kurzor nem villog

### Esc megszakitas (US6)

- Generalas kozben Esc → `generating = false` + should_cancel true
- A reszleges valasz megmarad a chat-ben
- A pipeline cancel kivetele NEM jelenik meg hibauzenetkent

### Kontextus meret ellenorzes

- Kuldes elott: becsult tokenszam (osszes uzenet + system prompt + uj) / 4
- Ha > 90% context limit → megerosito modal: becsult/max token, I/N
- Ha a pipeline megis hibat dob: baratsagos uzenet (nem technikai)

### Modalis ablakok

1. **help** (F1): Billentyuzet sugo
2. **quit_confirm** (F10): Kilepes megerosites (I/N)
3. **diagnostics** (F12): Pipeline + embedding reszletes allapot
4. **edit_confirm**: Szerkesztes megerosites — "X uzenet torlodni fog"
5. **context_overflow**: Kontextus tullpes — "~Xk/Yk token"

## Project Structure

### Uj es modositott fajlok

```text
SYSTEM_PROMPT.md                    # UJ: Szilvabot system prompt + stilusjegyek
config/masodik.json                 # MODOSITAS: nevek, prompt, history, gen. parameterek

include/masodik/tui/
├── chat_message.hpp                # MODOSITAS: name mezo hozzaadasa
├── chat_screen.hpp                 # MODOSITAS: browse_index parameter
├── chat_history.hpp                # UJ: JSONL load/append/rewrite
└── markdown_render.hpp             # UJ: markdown → FTXUI Element

src/tui/
├── chat_screen.cpp                 # MODOSITAS: nevek, markdown, browse scroll, osszevont megjelenitest
├── chat_history.cpp                # UJ: JSONL append + load + rewrite
├── markdown_render.cpp             # UJ: bold, kodblokk, inline kod, ujsor
└── help_screen.cpp                 # MODOSITAS: uj billentyuk

src/main/tui_app.cpp               # MODOSITAS: config, system prompt, history, browse, paste,
                                    #   edit confirm, context check, debug log

data/tui-debug.log                  # DEBUG: esemeny naplo (ArrowUp, Enter, send, generation)
data/chat-history.jsonl             # RUNTIME: JSONL chat history
.gitignore                          # MODOSITAS: data/ hozzaadva
CLAUDE.md                           # MODOSITAS: chat funkciok dokumentalva
```

## Billentyuzet osszefoglalo

| Billentyu | Normal mod | Browse mod | Generalas |
|-----------|-----------|------------|-----------|
| Enter | Kuldes | Betoltes mezobe | Blokkolt |
| Alt+Enter | Uj sor | Blokkolt | Blokkolt |
| Esc | — | Visszalep | Megszakit |
| ArrowUp | Browse (ures mezobol) | Elozo uzenet | Blokkolt |
| ArrowDown | — | Kov. uzenet/kilep | Blokkolt |
| Ctrl+U | Mezo torles | Blokkolt | Blokkolt |
| Ctrl+C | Hint (F10) | Hint (F10) | Hint (F10) |
| F1 | Sugo | Sugo | Sugo |
| F2 | Uj chat | Uj chat | — |
| F5 | Ujratolt | — | — |
| F9 | Web be/ki | — | — |
| F10 | Kilepes | Kilepes | Kilepes |
| F12 | Diagnosztika | Diagnosztika | Diagnosztika |
