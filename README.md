# 🏰⚔️ CLIRPG - Endless Rift

> **A terminal dungeon-crawler roguelike written in C++20 — no external libraries, no engine, just `make`.**

Descend an endless rift, clear floors, hoard loot, and push your build to the limit. Pure TTY flavor with a colorful themed UI (16-color ANSI) that degrades gracefully to plain ASCII in pipes, tests, and CI.

![Badge: C++20](https://img.shields.io/badge/C%2B%2B-20-blue) ![Badge: no-deps](https://img.shields.io/badge/dependencies-none-brightgreen) ![Badge: tests](https://img.shields.io/badge/tests-18614%20checks-success) ![Badge: platform](https://img.shields.io/badge/macOS-Linux-Windows-brightgreen)

> **New here?** Read [HOW_TO_PLAY.md](HOW_TO_PLAY.md) — a full class-by-class, mechanic-by-mechanic guide.

---

## 🎬 See it in action

That "themed UI" is real block-meters, HP bars, and colored legends — here's what a floor fight looks like (ASCII rendering):

```text
------------------------------ Round 1  ·  L1 -------------------------------
| HP ################ 40/40   Stamina ############ 14/14
------------------------------------------------------------------------------
| [1] Goblin HP ########## 13/13
[a]ttack [s]pell [i]tems [f]lee (h help) >   You strike Goblin for 6 damage.
  Goblin hits you for 4.
------------------------------ Round 2  ·  L1 -------------------------------
| HP ##############.. 36/40   Stamina ############ 14/14
| Belt:  [1] heal 2x  [2] -
-------------------------------------------------------------------------------
| [1] Goblin HP #####..... 7/13
[a]ttack [s]pell [i]tems [f]lee (h help) >   You strike Goblin for 8 damage.
Victory! +8 XP, +12 gold
    loot: Mana Draught [Uncommon Iron iLvl 1] restores 59 resource
```

---

## ✨ Features

### ⚔️ Combat

- **Turn-based fights** — attack, cast, potions (belt quick-keys `1`/`2` for an instant sip), and flee (75%). Potions **stack** and **scale with your floor** (55+22·floor HP / 45+14·floor mana). Every hit lands, capped at **60% of your max HP** so nothing can one-shot you. Status effects: Burn, Bleed, Poison, Stun, plus the boss's deadly Thrall summon. Opening a combat menu (potions/spells/help) and backing out costs **no turn and no enemy attack**.
- **5 classes · 12-spell trees** — Warrior, Mage, Rogue, Paladin, Necromancer (resource **Soul**, passive **Soul Harvest**, innate 10% life steal); each tree has depth-4 capstone spells, Element-tagged casters (🔥 Fire, ❄️ Frost, ✨ Arcane), and **Enfeeble / Vulnerable** debuffs. The Necromancer's **Arise, Skeleton Knight** is the game's first summon — a permanent minion that auto-attacks each round and absorbs hits meant for you (never counts as a kill, no XP/loot). The spell book & trainer use `<rank> <branch>` codes (`1 1` Backstab, `2 1` Vanish, `2 2` Corrosive Slash), and spell damage **scales with floor** on top of level, so casters stay relevant in deep descents. In combat you pick from your *learned* spells (`0` backs out).
- **Passive training** — at camp, dump spare skill points into permanent rows (Might +3 ATK, Vitality +10 HP, Focus +3 res, Tenacity +1 DEF, Fleetness +1 regen per rank, 5 ranks each).

### 📈 Progression

- **Loot that matters** — 5 rarities, 5 tiers (Iron → Void), 8 equipment slots incl. **trinkets** with relic powers with per-slot affixes, socketed **runes**, and set bonuses (Titanic, Infernal, Frostbound, Voidwalk).
- **Blacksmith & Merchant** — upgrade (cap = your floor), reforge/reroll, awaken, socket & extract runestones; buy, sell, and salvage gear.
- **4-currency economy** — gold, shards, essence, runestones.
- **Ascend the Rift** — reach Floor 100 to prestige: +1 Aspect that heightens the darkness, and a permanent perk (Heirloom, Runeforge, Insight, …).
- **Living bestiary & achievements** — per-name kill records, boss slays, biome & affix logs, and an achievements wall.

### 🎛 Quality of life

- **Robust save system** — `RPGSAVE v10` (checksummed), three slots (`save1-3.rpg`), roguelike death rules. Inspect or hand-verify it in any text editor.
- **Softcore & Hardcore** — start a run soft (death costs 20% gold and a floor, the vault endures) or hardcore (death erases the save and your hero is etched forever into the Glory track).
- **Achievement track** — an account-wide **Glory** list in `glory.rpg` (15 achievements, checksummed): unlocks persist even after a slot is overwritten, and every fallen hardcore hero is remembered.
- **Plain mode / `NO_COLOR`** — full ANSI on a TTY; clean ASCII in pipes, terminals that say `dumb`, or with `--plain`. Every glyph has an ASCII twin (e.g. ◆ healing → `o`), so the game reads fine with zero Unicode. Standard `NO_COLOR` respected.
- **Tested hard** — 18,614 assertions across the suite, ASan + UBSan-clean, MSYS2 GCC Windows CI gate.
- **A coherent panel grammar** — semantic color tokens (`ui::c::`), one boxed flow for every menu/event screen, `chip + glyph + label` rows throughout the camp and merchant, and width-aware `wrap()` guards so panels never outgrow the terminal in plain mode.

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
| `s` | Cast a learned spell (`0` backs out) |
| `i` | Use a potion — belt slots listed first, `0` backs out |
| `f` | Flee the fight |
| `1` / `2` | Drink that belt potion instantly |
| `h` | In-fight help |

**At camp:**

| Key | Action |
| --- | ------ |
| `1` | Rest (full recovery) |
| `2` | Blacksmith — upgrade / reforge / awaken / runes |
| `3` | Merchant — buy / sell / salvage |
| `4` | Equip gear |
| `5` | Belt — bind quick potions to the `1`/`2` keys |
| `6` | Trainer — learn spells, or `<P>` passive training |
| `7` | Respec — refund the tree |
| `8` | Inventory — browse gear, potions & runestones |
| `9` | Descend — head deeper |
| `10` | Records & Codex |
| `11` | Ascend the Rift *(appears at Floor 100)* |
| `0` | Save & quit |

**Your first five minutes:** fight → grab loot → camp → blacksmith/merchant to equip and prep → descend → repeat. Every 25th floor grants **Rift Mastery**; every 5th is a `<BOSS>` fight; Floor 100 unlocks **Ascend the Rift** (prestige +1).

---

## 💾 Save format

Saves are plain text, keyed `=` values, behind an XOR checksum. `cat save1.rpg` looks like this:

```text
RPGSAVE v10
[character]
class=0
level=1
xp=15
skillPoints=2
hp=33
resource=14
floor=1
unlocked=000000000000
train=0 1 2 0 0
hardcore=0
[vault]
gold=76
shards=1
bestFloor=1
...
```

Every field is human-readable — kill records, perks, belt kinds (`belt=3 0`), potion stacks (`...|kind|count`), training ranks, the hardcore flag, the whole bestiary are just lines. Versioned (`kVersion`) and checksummed, so corrupt or foreign files are rejected cleanly.

---

## 🧪 Tests & sanitizers

```sh
make tests      # full suite → prints "18614 checks, 0 failures"
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
| `src/items.cpp` | Loot gen, runes, crafting, potion stacks & belt kinds |
| `src/save.cpp` | Checksummed `RPGSAVE v10` persistence |
| `src/glory.cpp` | Account-wide achievement track + fallen heroes (`glory.rpg`) |
| `src/core.cpp` | Shared formulas, constants, RNG |
| `src/io.cpp` | Input handling (pipe-tolerant) |
| `src/tests.cpp` | The 18,614-assertion harness |
| `Makefile` | all / run / tests / asan / tests_asan / clean |

---

## 🤝 Contributing

PRs and issues are welcome. If you touch code, make sure `make`, `make tests`, and `make tests_asan` are all green before opening the pull request.

## 📄 License

[MIT](./LICENSE) — © 2026 Dapolet.

---

## ❤️ Why?

Because sometimes the best dungeon is one you can `cat` into a pipe. Zero dependencies, character-build depth, and a save file you could hand-verify in a text editor.