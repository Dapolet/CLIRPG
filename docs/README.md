# Endless Rift — CLI RPG

A terminal dungeon-crawler written in C++20 with a themed, colorful UI that
falls back to clean plain-ASCII when stdout is not a real terminal (pipes,
tests, CI) or when you pass `--plain`.

## Requirements

- A C++20 compiler with `g++`-compatible CLI (`g++`, `clang++`, `-std=c++20` in
  the generated Makefile; `make` defaults to `g++`). On macOS, Apple Clang
  works fine.
- `make` (GNU or BSD).
- Nothing else — no external libraries. A terminal that supports ANSI true-ish
  colors (24-bit) for the full look; everything degrades gracefully otherwise.

## Build

From the project root (`/Users/Dapolet/VSCode/RPG`):

```sh
make         # or: make all  — builds ./build/rpg with -O2, -Wall -Wextra -Wpedantic
```

The build is warning-clean (`-Wpedantic` included); a non-zero exit or any
`warning:` line means something is wrong.

## Run

```sh
make run          # build (if needed) then start the game
# or directly:
./build/rpg
```

### Flags

`./build/rpg` accepts:

- `--plain`, `--no-color`, `-p` — force plain ASCII output (no ANSI codes).
  Useful for logging, scripts, or terminals that can't render bold/color.
- The game itself auto-detects: if stdout is **not** a TTY (piped, redirected,
  in a test harness), or `TERM` is `dumb`, or `NO_COLOR` is set — ANSI is
  disabled automatically potenti/ho so scripted runs stay clean.

Colors/glyphs are also re-enabled appropriately once an interactive TTY is
detected. Panels auto-size between 44–100 columns based on your terminal width.

### Controls (in-game), abbreviated

Combat: `a`ttack · `s`pell · `i`tems · `f`lee · `=` auto (toggle) · `h` help.
Items/menus: `i`nventory · `e`quip · `b`elt · `l`oadouts · `r`unes · `s`ets ·
`c`raft · number/`0` back. Save slots `save1.rpg` .. `save3.rpg`.

## Tests

```sh
make tests     # compiles tests.cpp + game sources, runs the full suite
```

The harness runs 11,921 checks (monotonicity of XP/enemy scaling, rarity drop
weights, rune socket caps, 12-spell class trees incl. Enfeeble/Vulnerable
debuff spells, save v6 roundtrip equality, set-piece counts, belt
/loadout/perk/bestiary persistence, corrupt-save rejection) and prints e.g.:

```text
11921 checks, 0 failures
```

Output is rendered through the plain path (stdout is a pipe in the harness),
so tests don't depend on your terminal.

## Sanitizers (ASan + UBSan)

```sh
make asan       # builds ./build/rpg_asan with -fsanitize=address,undefined
./build/rpg_asan # run an ASan/UBSan-instrumented playthrough
```

A clean run exits `0` with no `ERROR: AddressSanitizer:` / `runtime error:`
output. This is the go-to for memory errors before committing.

## Cleanup

```sh
make clean      # removes build/ (objects, binaries — NOT your save files)
rm saveN.rpg    # delete a specific saved run (created by the game)
```

Stray save files (`save1.rpg` …) are *not* produced by the build — they only
appear when you actually play and save. The test/ASan harnesses clean up after
themselves; a stray `saveN.rpg` in the repo root usually means an interrupted
interactive session.

## Save format

Versioned (`RPGSAVE v6`), line-keyed sections — `[character]`, `[vault]`
(incl. rune sockets, potion belt quick-slot uids, loadouts, ascension perks,
name-keyed bestiary, records), `[sig]` XOR checksum. Save-time is stamped on
write and surfaced in the save-slot summary. Corrupt or older-version files are
rejected cleanly; older saves load as a fresh character in a new slot.

## File layout

All sources live in `src/`; the build puts objects + binaries in `build/`
(cleanly ignored by git).

- `src/main.cpp` — game loop + menu; parses `--plain`
- `src/ui.cpp` / `src/ui.hpp` — themed panels, glyphs, colors, plain mode
- `src/combat.cpp` — turn-based combat, status effects (incl. Enfeeble /
  Vulnerable debuffs), bestiary/affix registration
- `src/character.cpp` — classes, 12-spell trees (depth-4 capstones), stats
- `src/items.cpp` — items, sockets, runes, belt/loadouts/perks, crafting
- `src/save.cpp` — v6 persistence roundtrip
- `src/core.cpp` — shared formulas/constants/rng
- `src/io.cpp` — line input (piped-tolerant)
- `src/tests.cpp` — `make tests` harness (11,921 checks)
- `docs/PLAN.md` — full design doc (v6)
