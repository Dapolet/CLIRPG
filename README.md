# 🏰⚔️ CLIRPG - Endless Rift

> **A terminal dungeon-crawler roguelike written in C++20 — no external libraries, no engine, just `make`.**

Descend an endless rift, clear floors, hoard loot, and push your build to the limit. Pure TTY flavor with a colorful themed UI (ANSI true-color) that degrades gracefully to plain ASCII in pipes, tests, and CI.

![Badge: C++20](https://img.shields.io/badge/C%2B%2B-20-blue) ![Badge: no-deps](https://img.shields.io/badge/dependencies-none-brightgreen) ![Badge: tests](https://img.shields.io/badge/tests-12%2C782%20checks-success) ![Badge: platform](https://img.shields.io/badge/macOS-Linux-Windows-brightgreen)

---

## ✨ Features

- ⚔️ **Turn-based combat** — attack, cast, potions, flee, auto-attack toggle; status effects: Burn, Bleed, Poison, Stun, + the boss's deadly Thrall summon.
- 🧙 **3 classes · 12-spell skill trees** — Warrior, Mage, Rogue; each tree has depth-4 capstone spells plus Element-tagged casters (🔥 Fire, ❄️ Frost, ✨ Arcane) and **Enfeeble / Vulnerable** debuffs.
- 🎒 **Loot that matters** — 5 rarities, 5 tiers (Iron → Void), per-slot affixes, socketed **runes**, and set bonuses (Titanic, Infernal, Frostbound, Voidwalk).
- 🔨 **Blacksmith** — upgrade, reroll (reforge), awaken, socket & extract runestones.
- 🏪 **Merchant** — buy potions, sell & salvage gear, rune economy.
- 🪙 **4-currency economy** — gold, shards, essence, runestones.
- 💾 **Robust save system** — versioned `RPGSAVE v6`, checksummed, per-slot (`save1-3.rpg`), death-penalty roguelike rules.
- 📖 **Living bestiary & achievements** — kill records by name, boss kills, affix/biome logs, and an achievements wall.
- 🧪 **Tested hard** — 12,782 checks, ASan + UBSan-clean, MSYS2 GCC Windows CI gate.
- 🌈 **Plain mode** — full ANSI when you're on a TTY; clean ASCII in pipes / `NO_COLOR` / `--plain`.

---

## 🚀 Quick start

Prerequisites: a C++20 compiler (`g++` / `clang++`), `make`. That's it.

```sh
make          # builds ./build/clirpg (warning-clean: -O2 -Wall -Wextra -Wpedantic)
make run      # build (if needed) then play
./build/clirpg   # or run directly
```

```sh
./build/clirpg --plain     # force plain-ASCII output (no ANSI codes)
./build/clirpg --no-color  # same, alias
```

The build must compile with **zero warnings** — any `warning:` line is a bug.

---

## 🎮 Controls

| Key | Action |
| --- | ------ |
| `a` | Attack |
| `s` | Cast a spell |
| `i` | Use an item (potions) |
| `f` | Flee the fight |
| `=` | Toggle auto-attack |
| `h` | Help |
| `1`–`9` / `0` | Menus: rest, blacksmith, merchant, gear, trainer, respec, descend, records, **ascend at Floor 100** |

Run the loop: **camp → blacksmith/merchant/trainer → descend → fight → repeat**. Reach Floor 100 to **Ascend the Rift** (prestige +1).

---

## 🧪 Tests & sanitizers

```sh
make tests       # full suite → prints "12782 checks, 0 failures"
make asan        # ASan+UBSan build of the game
./build/clirpg_asan # play through it clean
make tests_asan  # whole test suite under ASan+UBSan
```

`make clean` removes `build/` only — your save files are never touched.

---

## 📁 Layout

| Path | What it is |
| --- | ---------- |
| `src/main.cpp` | Game loop, menus, `--plain` parsing |
| `src/ui.cpp` | Themed panels, glyphs, colors, camp |
| `src/combat.cpp` | Combat engine + status effects |
| `src/character.cpp` | Classes, spell trees, stats |
| `src/items.cpp` | Loot gen, runes, crafting, vault |
| `src/save.cpp` | Checksummed `v6` persistence |
| `src/core.cpp` | Shared formulas, constants, RNG |
| `src/io.cpp` | Input handling (pipe-tolerant) |
| `src/tests.cpp` | The 12,782-check harness |
| `Makefile` | all / tests / asan / tests_asan / run / clean |
| `docs/` | Local planning notes (gitignored, not on GitHub) |

---

## ❤️ Why?

Because sometimes the best dungeon is one you can `cat` into a pipe. Zero dependencies, character-build depth, and a save file you could hand-verify in a text editor.
