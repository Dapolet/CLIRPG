# Endless CLI RPG — Implementation Plan (v5)

An endless, turn-based CLI RPG in C++20. Pick a class, fight endlessly-scaling
enemies and bosses, loot gear with rarity + enchantments + **runestones**,
reforge/upgrade it to track your floor, spend spell-tree points, and die softly
while your **Vault** (gear, materials, gold) always survives. Long-run
progression is powered by compounding **Rift Mastery** milestones and prestige
**Rift Aspects** so the difficulty never hard-walls.

## 1. Deliverables & File Layout

```
/Users/Dapolet/VSCode/RPG/
  src/       — all C++ sources: main.cpp, ui.*, combat.*, character.*,
               items.*, save.*, io.*, core.*, bestiary.hpp, tests.cpp
  build/     — generated: *.o / *.d, rpg / tests / rpg_asan / tests_asan
  docs/      — PLAN.md (this plan, v6), README.md
  Makefile   — targets: all / tests / asan / tests_asan / run / clean
  .github/workflows/build.yml — msys2 Windows + macOS CI gates
```

Build: `g++ -std=c++20 -Wall -Wextra -Wpedantic`. No external dependencies.
Namespaces: `rpg::core`, `rpg`, `rpg::combat`, `rpg::save`, `rpg::ui`, `rpg::io`.

## 2. Architecture

Layered, dependencies point downward only:

```
ui  (menu → slots → camp ⇄ combat[sell/salvage/sockets])
 └── combat (turn loop, status effects, enemies)
      └── character (classes, spell trees, passives)
           └── items (loot, reforge, runestones)   ← Vault lives here
                └── core (RNG + formula constants + Element)
io shared between ui and combat (no upward deps)
save imports character + items
```

C++ idioms (per cpp-pro skill):

- `enum class` for Rarity / Slot / ItemTier / AffixType / StatusEffect /
   ClassId / EnemyAffix / SpellType / Element / RuneType / CampResult.
- Spell trees are 12 spells/class (4 capstones; index = branch*4 + depth);
  built once and cached (`classSpells` returns `const std::vector<Spell>&`).
- Spell `Spell{...}` designators MUST follow the struct's declaration order
  (name, type, cost, cooldown, potency, lvlScale, hits, healPower, buffAttack,
  buffDefense, buffTurns, effect, effectTurns, armorShred, stunChancePct,
  critBonusSelf, element, enemyAtkDownPct, enemyVulnPct).
  ClassId / EnemyAffix / SpellType / Element / RuneType / CampResult.
- Static data tables as `constexpr std::array`; class spell trees built once
  and **cached** (`classSpells` returns `const std::vector<Spell>&`).
- `Rng` (`mt19937_64`, seeded once, RAII-owned) — never a global.
- Value semantics for `Item`/`Spell`/`Rune` — no raw ownership.
- Const-correct throughout.

## 3. Core Formulas (tunable constants in core.cpp)

```
xpNeeded(level)     = 40 * level^1.6
enemyScale(floor)   = exp( ln(1.07) * (floor-1) )         // gentler 1.07 curve
aspectScale(aspect) = 1 + 0.15 * aspect                    // prestige difficulty
elementMult(a,t)    = 1.0  (None involved) |
                      0.7  (same element)   |
                      1.3  (Fire>Frost, Frost>Arcane, Arcane>Fire) | 1.0
rawDamage(power)    = power * roll(0.9, 1.1)
mitigated          = raw * 100 / (100 + defense)
critDamage         = raw * (1.5 + critBonus/100)
enemy.hpMax        = base.hp * eliteMult * enemyScale * aspectScale * roll(0.9,1.1)
enemy.attack       = base.atk * eliteMult * enemyScale * aspectScale
enemy.defense      = base.def + floor/4
xpReward           = base.xp * ceil(enemyScale) * (boss ? 5 : 1) * (elite ? 2 : 1)
goldReward         = 12 * floor * roll(0.8,1.2) * aspectScale   // boss: 60*floor
```

### Rarity drop weights (rebalanced every floor)

| Rarity    | affix slots | weight(floor)                 |
|-----------|-------------|-------------------------------|
| Common    | 1           | max(100 − 3·floor, 5)         |
| Uncommon  | 1           | 55                            |
| Rare      | 2           | min(20 + floor, 60)           |
| Epic      | 3           | min(4 + floor/2, 25)          |
| Legendary | 4           | min(floor/3, 8)               |

**Boss loot guarantees one Epic+ piece** (`makeGear(floor, rng, Rarity::Epic)`).

### Drop iLvl vs upgrade

- Drops LAG the floor: `iLvl = max(floor − roll(0, gap), 1)`,
  `gap = min(floor/4, 6)` — so the blacksmith `upgrade` (pulls iLvl up to the
  floor cap) is the catch-up tool it was meant to be.

### Base material tiers (unlock by floor)

Iron(1) → Steel(8) → Mythril(18) → Adamant(35) → Void(60).

### Reforge / Upgrade / Awaken / Respec / Rune costs

```
reforgeCost   = 25·(rarity+1)·(1 + iLvl/8)   (gold)
upgradeCost   = 5·(tier+1) + iLvl/2          (shards; +1 iLvl, capped at floor)
awakenCost    = 50·(rarity+1)·(1 + iLvl/8)   (essence; adds affix slot, rare+)
respecCost    = 25 + 15·level                (gold)
runeExtract   = 10 + iLvl                    (gold)      // socketing is free
sellPrice     = 8·iLvl·(rarity+1)/2          (gold)      // merchant buys gear
salvageShards = 1 + iLvl/20 + 2·(rarity)     // epic+ also yields 1 essence
```

### Death (soft — kept as-is)

- Vault (gear + equipment + runes + materials + gold) is **fully preserved**.
- Character keeps level/XP/tree, loses **20% of gold**, drops **1 floor**,
  respawns full HP/resource at camp. `deaths` record increments.
- No escalating penalties (user decision).

## 4. Compounding Rift Mastery (the wall fix)

- Player power currently grows ≈linearly with level+gear while enemy HP grows
  exponentially → hard wall near floor 60–75. The enemy curve is damped to
  1.07, **and** the player gains permanent compounding multipliers:
  - `Vault.mastery` (+1 when a floor milestone 25/50/75/… is cleared).
  - Applied in `EffectiveStats`:
    `attack ×= 1.08^M`, `maxHp ×= 1.06^M`, `maxResource ×= 1.05^M`.
  - Stats cap math: every heal/restore path caps against the **effective**
    maxHp/maxResource (mastery and HP% affixes actually work now).
- Result: M ≈ floor/25 → player growth e^(0.077·floor) vs enemy e^(0.068·floor)
  — slowly farmable into the several-hundreds of floors.

## 5. Rift Aspect (prestige, floor 100+)

- At camp with floor ≥ 100 the player can **Ascend the Rift**: `aspect++`,
  floor→1, full recovery, Vault records preserved.
- Every enemy after that scales by `aspectScale(aspect) = 1 + 0.15·aspect`;
  gold rewards echo it. Achievements and records track ascensions.

## 6. Class passives & elemental affinities

- **Warrior — Adrenaline**: taking damage grants +15% damage on the next
  action (stacks ×3, consumed on the next attack/spell).
- **Mage — Arcane Flow**: +2 resource regen per turn (others +1).
- **Rogue — First Strike**: a free opening round (enemies do not act round 1)
  and +25 crit on the first action.
- **Elements**: Mage spells carry Fire / Frost / Arcane; elemental enemies get
  an alignment (Cultist=Arcane, Wraith=Frost, Imp=Fire, + random at floor≥6).
  `elementMult` table applies — matching element ×0.7, counter ×1.3.
- **Debuffs (Enfeeble / Vulnerable)**: each class has a depth-4 capstone debuff
  spell (Warrior "Cripple" = Enfeeble, Mage "Hex" = Vulnerable, Rogue
  "Crippling Venom" = Enfeeble+Poison). `enemyAtkDownPct` applies Enfeeble
  (enemy attack ×(100−pct)/100 for N turns); `enemyVulnPct` applies Vulnerable
  (enemy takes ×(100+pct)/100 damage for N turns). Applied on hits of both
  basic attacks and spells; Vulnerable multiplies all damage to the target.

## 7. Runestones (socketables)

- `RuneType`: Power (dmg%) · Vitality (maxHp%) · Warding (def%) · Flow
  (resource regen) · Finesse (crit). Values scale with drop floor.
- Gear sockets by rarity: Common 0 / Uncommon 1 / Rare 1 / Epic 2 / Legendary 3.
- Bosses and (rarely) normal fights drop runestones into the Vault.
- Blacksmith: **socket for free**, **extract for `10 + iLvl` gold** (reusable).
- Sockets show as `◆/◇`; bonuses stack into `EffectiveStats` via equipped gear.

## 8. Economy & QoL

- **Merchant** buys gear (`sellPrice`) and salvages it into shards (essence for
  epic+); potions/materials still for sale. Vault stays uncapped.
- **Auto-attack toggle** in combat: press `=` to repeat basic attacks (one
  compact line per round) until HP <35%, a boss enrages, or a minion spawns —
  then control returns.
- **3 save slots** (`save1.rpg` … `save3.rpg`), slot picker with character
  summaries in New/Continue, overwrite confirmation.

## 9. Records & Achievements (persisted)

- Vault: `bestFloor, bossesSlain, kills, deaths, legendaryFound, mastery,
  aspect` — saved with the Vault, survive death forever.
- Camp “Records & Achievements” screen derives achievements:
  First Blood, Boss Slayer, Deep Delver (floor 25), Dungeon Master (floor 50),
  Riftbreaker (floor 100), Legendary Hunter, Ascendant, Master of the Rift.

## 10. Save Format (`saveN.rpg` v6)

Versioned (`RPGSAVE v6`), line-keyed sections: `[version]`, `[character]`,
`[vault]` (incl. runs, belt quick-slots, loadouts, perks, bestiary, records),
`[sig]` (XOR checksum). Items serialize their socketed runes plus a stable
`uid` and optional `setTag`; belt/loadout fields and the bestiary (name-keyed
maps — future-proof) ride along so new content never invalidates a save.
`[character]` carries class/level/xp/skillPoints/hp/resource/floor/unlocked
(12-slot spell tree) aspects; `saveTime` + total gold earned are stamped on
write and surfaced in save-slot summaries. Corrupt/unknown/older-version
files are rejected cleanly (v5 and earlier saves are rejected).

## 11. Test Matrix (tests.cpp)

- xpNeeded / enemyScale monotonicity · element table corners
- drop iLvl lags floor & is upgrade-reachable (the A1 fix)
- boss loot contains an Epic+ item (the A2 fix)
- spellHeal scales with level (the A3 fix)
- classSpells cache / spell lookup — 12 spells/class (the A4 fix)
- every class owns ≥1 Enfeeble/Vulnerable debuff spell; depth-3 capstones
  unlock in declaration order (Enfeeble before Vulnerable, lvlScale before
  healPower, etc. — the Windows msys2 GCC regression guard)
- mastery multipliers monotonic in stats
- rune socket cap (by rarity), bind/extract roundtrip, save roundtrip incl.
  runes + records
- heal/restore honor effective caps (Hp%/mana affixes + mastery)
- save v5 roundtrip equality (version/character/vault + belt, loadouts,
  perks, bestiary, setTag, uid, saveTime) · corrupt/older rejected · peek
  summaries show class/level/floor/saveTime
- belt/potion persistence: `Vault::add` uid assignment, belt quick slots
  survive write/read, useBelt/returnBelt/saveLoadout/equipLoadout roundtrip
- set-piece counts (`setPieces`), set/perk names, badge helper roundtrips
- death preserves vault & records; auto-attack can’t kill the player when it
  cedes control (scripted through fight)
- `make tests` + `make asan` + scripted playthroughs clean