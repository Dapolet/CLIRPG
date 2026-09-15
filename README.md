# 🏰⚔️ CLIRPG - Endless Rift

> **A terminal dungeon-crawler roguelike written in C++20 — no external libraries, no engine, just `make`.**

Descend an endless rift, clear floors, hoard loot, and push your build to the limit. Pure TTY flavor with a colorful themed UI (16-color ANSI) that degrades gracefully to plain ASCII in pipes, tests, and CI.

![Badge: C++20](https://img.shields.io/badge/C%2B%2B-20-blue) ![Badge: no-deps](https://img.shields.io/badge/dependencies-none-brightgreen) ![Badge: tests](https://img.shields.io/badge/tests-12582%20checks-success) ![Badge: platform](https://img.shields.io/badge/macOS-Linux-Windows-brightgreen)

---

## 🎬 See it in action

That "themed UI" is real block-meters, HP bars, and colored legends — here's what a floor fight looks like (ASCII rendering):

```text
------------------------------ Round 1  ·  L1 -------------------------------
| HP ################ 40/40   Stamina ############ 14/14
------------------------------------------------------------------------------
| [1] Goblin HP ########## 13/13
[a]ttack [s]pell [i]tems [f]lee  (= auto) (h help) >   You strike Goblin for 6 damage.
  Goblin hits you for 4.
------------------------------ Round 2  ·  L1 -------------------------------
| HP ##############.. 36/40   Stamina ############ 14/14
| Adrenaline x1
------------------------------------------------------------------------------
| [1] Goblin HP #####..... 7/13
[a]ttack [s]pell [i]tems [f]lee  (= auto) (h help) >   You strike Goblin for 8 damage.
Victory! +8 XP, +12 gold
    loot: Mana Draught [Uncommon Iron iLvl 1] restores 59 resource
```

---

## ✨ Features

### ⚔️ Combat

- **Turn-based fights** — attack, cast, potions (with belt quick-slots), flee, and a `=` auto-attack toggle. Status effects: Burn, Bleed, Poison, Stun, plus the boss's deadly Thrall summon.
- **3 classes · 12-spell trees** — Warrior, Mage, Rogue; each tree has depth-4 capstone spells, Element-tagged casters (🔥 Fire, ❄️ Frost, ✨ Arcane), and **Enfeeble / Vulnerable** debuffs.

### 📈 Progression

- **Loot that matters** — 5 rarities, 5 tiers (Iron → Void), 7 equipment slots with per-slot affixes, socketed **runes**, and set bonuses (Titanic, Infernal, Frostbound, Voidwalk).
- **Blacksmith & Merchant** — upgrade (cap = your floor), reforge/reroll, awaken, socket & extract runestones; buy, sell, and salvage gear.
- **4-currency economy** — gold, shards, essence, runestones.
- **Ascend the Rift** — reach Floor 100 to prestige: +1 Aspect that heightens the darkness, and a permanent perk (Heirloom, Runeforge, Insight, …).
- **Living bestiary & achievements** — per-name kill records, boss slays, biome & affix logs, and an achievements wall.

### 🎛 Quality of life

- **Robust save system** — `RPGSAVE v7` (checksummed), three slots (`save1-3.rpg`), roguelike death rules. Inspect or hand-verify it in any text editor.
- **Plain mode / `NO_COLOR`** — full ANSI on a TTY; clean ASCII in pipes, terminals that say `dumb`, or with `--plain`. Standard `NO_COLOR` respected.
- **Tested hard** — 12,582 assertions across the suite, ASan + UBSan-clean, MSYS2 GCC Windows CI gate.

---

## 🚀 Quick start

Prerequisites: a C++20 compiler (`g++ ≥ 11` / `clang++ ≥ 14`) and `make`. No packages, no CMake, no vcpkg.

```sh
make          # builds ./build/clirpg (warning-clean: -O2 -Wall -Wextra -Wpedantic)
make run      # build (if needed) then play
./build/clirpg   # or run directly
```

**Windows**: same thing under MSYS2 (UCRT64) — `make` produces `./build/clirpg.exe`.

Force plain output (pipes, scripts, or terminal torture):

```sh
./build/clirpg --plain     # force plain-ASCII output (no ANSI codes)
./build/clirpg --no-color  # same, alias
./build/clirpg -p          # same, shortest
```

The build must compile with **zero warnings** — any `warning:` line is a bug.

---

## 🎮 Controls

**In a fight:**

| Key | Action |
| --- | ------ |
| `a` | Attack |
| `s` | Cast a spell |
| `i` | Use a potion (belt slots listed first) |
| `f` | Flee the fight |
| `=` | Toggle auto-attack |
| `h` | In-fight help |

**At camp:**

| Key | Action |
| --- | ------ |
| `1` | Rest (full recovery) |
| `2` | Blacksmith — upgrade / reforge / awaken / runes |
| `3` | Merchant — buy / sell / salvage |
| `4` | Manage gear — equip & loadout save/equip |
| `5` | Belt — bind quick potions |
| `6` | Spell trainer — spend skill points |
| `7` | Respec — refund the tree |
| `8` | Descend — head deeper |
| `9` | Records & Codex |
| `10` | Ascend the Rift *(appears at Floor 100)* |
| `0` | Save & quit |

**Your first five minutes:** fight → grab loot → camp → blacksmith/merchant to equip and prep → descend → repeat. Every 25th floor grants **Rift Mastery**; every 5th is a `<BOSS>` fight; Floor 100 unlocks **Ascend the Rift** (prestige +1).

---

## 💾 Save format

Saves are plain text, keyed `=` values, behind an XOR checksum. `cat save1.rpg` looks like this:

```text
RPGSAVE v7
[character]
class=0
level=1
xp=15
skillPoints=2
hp=33
resource=14
floor=1
unlocked=000000000000
[vault]
gold=76
shards=1
bestFloor=1
...
[sig]
42   (XOR checksum of everything above; changes with the payload)
```

Every field is human-readable — kill records, perks, belt, loadouts, the whole bestiary are just lines. Versioned (`kVersion`) and checksummed, so corrupt or foreign files are rejected cleanly.

---

## 🧪 Tests & sanitizers

```sh
make tests       # full suite → prints "12582 checks, 0 failures"
make asan        # ASan+UBSan build of the game
./build/clirpg_asan   # play through it clean
make tests_asan  # whole test suite under ASan+UBSan
```

One "check" = one assertion; the suite is a set of test cases that script stdout/stdin, save round-trips, RNG seeds, and full fights.

`make clean` removes `build/` only — your save files are never touched.

---

## 📁 Layout

| Path | What it is |
| --- | ---------- |
| `src/main.cpp` | Menus, new-game/continue orchestration, `--plain` parsing |
| `src/game.cpp` | The adventure loop — floors, encounters, rewards, persistence |
| `src/ui.cpp` | Themed panels, glyphs, colors, camp & shop screens |
| `src/combat.cpp` | Combat engine + status effects + biome/encounter tables |
| `src/character.cpp` | Classes, spell trees, stats |
| `src/items.cpp` | Loot gen, runes, crafting, 7-slot vault & loadouts |
| `src/save.cpp` | Checksummed `RPGSAVE v7` persistence |
| `src/core.cpp` | Shared formulas, constants, RNG |
| `src/io.cpp` | Input handling (pipe-tolerant) |
| `src/tests.cpp` | The 12,582-assertion harness |
| `Makefile` | all / run / tests / asan / tests_asan / clean |

---

## 🤝 Contributing

PRs and issues are welcome. If you touch code, make sure `make`, `make tests`, and `make tests_asan` are all green before opening the pull request.

## 📄 License

[MIT](./LICENSE) — © 2026 Dapolet.

---

## ❤️ Why?

Because sometimes the best dungeon is one you can `cat` into a pipe. Zero dependencies, character-build depth, and a save file you could hand-verify in a text editor.