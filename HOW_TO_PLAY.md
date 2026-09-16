# How To Play CLIRPG — Endless Rift

*A terminal dungeon-crawler roguelike in C++20 — no dependencies, just `make`.*

You descend an endless rift, floor by floor, looting gear and building a spell kit, until
your hero Ascends, falls, or quits. This guide is the full manual: every mechanic, every
screen, and every number that matters.

---

## Quick start (60 seconds)

1. `make && ./build/clirpg`
2. Main menu → **1 New Game** → **1 Slot 1** → pick a class → pick **Softcore** (until you
   know the ropes).
3. Type `a` to attack. Win the two fights on Floor 1, then use the **camp**:
   - **6 Trainer** → learn `1 1` (your opening spell) → `0` exits.
   - **5 Belt** → bind a potion to slot 1 so `1` drinks it in combat.
   - **1 Rest** if you're hurt.
4. **9 Descend** → repeat. The first **boss** hits at Floor 5 — reach it with a learned
   spell, decent gear, and full HP.

Everything else below is the fine print.

---

## Running the game

| Command | What it is |
| --- | --- |
| `make` | Builds `./build/clirpg` (zero warnings). |
| `./build/clirpg` | Play. |
| `./build/clirpg --plain` (or `-p` / `--no-color`) | No ANSI color, ASCII glyphs — for logs, tests, SSH-over-pipes, screenshots. |
| `make tests` | 12,732-check test suite. |
| `make tests_asan` | The same suite under ASan/UBSan. |

Panel width follows the terminal (`COLUMNS` or ioctl), clamped between **44 and 100
columns**; panels wrap and never exceed it. The codebase is MSYS2-GCC clean (Windows CI).

---

## The core loop

1. **Main menu** → New Game / Continue / Quit.
2. **Hero creation**: save slot → class → mode (softcore/hardcore) → (on Ascension) a Rift
   Aspect perk.
3. **A floor**: ~28% chance of a **floor event**, then `2 + floor/6` encounters
   (capped at 5). Every **5th floor** the *last* encounter is a **boss**.
4. **Camp** (after the encounters) — the hub: rest, blacksmith, merchant, equip, belt,
   trainer, respec, inventory, descend, records, save & quit.
5. Every **25 floors** the Rift deepens (**Rift Mastery +1**, permanent compounding
   power). At **Floor 100+** the camp offers **Ascend** — prestige.

---

## Hero creation

### Save slots

`slot1.rpg`/`slot2.rpg`/`slot3.rpg` — one hero each. Continue loads a slot; the slot
line shows class, level, floor, and hardcore status. Starting New Game on an occupied
slot overwrites it (your call).

### Classes

| | Warrior | Mage | Rogue |
| --- | --- | --- | --- |
| HP | 40 + 7/level | 26 + 4/level | 30 + 5/level |
| Resource | Stamina (14 + 2/lvl) | Mana (20 + 3/lvl) | Energy (16 + 3/lvl) |
| Resource regen/turn | +1 | +2 | +1 |
| ATK | 7 + 2/lvl | 4 + 1/lvl | 6 + 2/lvl |
| DEF | 3 + 1/lvl | 1 + (lvl-1)/2 | 2 + (lvl-1)/2 |
| Crit chance | 5% | 5% | 10 + (lvl-1)/4 % |
| Crit bonus dmg | +10% | +15% | +20% |
| Passive | **Adrenaline** — each hit you take gives +15% to your next attack (stacks). | **Arcane Flow** — faster mana regen. | **First Strike** — +25% crit on round 1. |

**Spell branches per class** (3 branches x 4 ranks each — see *Spells* for the full list):

| Class | Branch 1 | Branch 2 | Branch 3 |
| --- | --- | --- | --- |
| Warrior | Berserker (damage) | Defender (tank/self-buff) | Warcry (support/debuffs) |
| Mage | Fire | Frost | Arcane |
| Rogue | Shadow (crit burst) | Poison (DoT/debuffs) | Tricks (multi-hit/burst) |

### Modes

- **Softcore** — death is a *setback*: take 20% gold damage, drop back a floor, keep the
  hero and everything else. Best floor is still tracked.
- **Hardcore** — death is *final*: the save is erased and the hero becomes a **Fallen**
  entry in the Glory track (class, level, floor, kills, gold earned). Hardcore heroes are
  marked on the slot screen and greeted on the menu.

---

## Stats & the character sheet

`showCharacter` prints on every camp visit:

```
HP #### 40/40          resource bar + current/max
XP #### 15/52  2 skill pt
ATK 7  DEF 3  Crit 5%(+10)  Leech 0%  XP+0%
Passive  Adrenaline
Gold 74  Shards 1  Essence 0  Runestones 0
Equipped: Weapon/ Armor/ Ring/ Amulet/ Helm/ Gloves/ Boots
```

The `(+N)` next to Crit is the *crit bonus* damage multiplier; Leech and XP% come from
affixes/runes/perks. Everything you see here is what combat actually uses.

### How the numbers work

- **Damage roll**: `ATK x (0.9..1.1)` per attack.
- **Defense mitigation**: `raw x 100 / (100 + DEF)` — 100 DEF halves damage.
- **Critical**: `damage x (1.5 + critBonus%)`.
- **Elements**: *Fire → Frost → Arcane → Fire*. Weak to your element = `x1.3`, same
  element = `x0.7`, no element = neutral.
- **XP needed per level**: `ceil(52 x level^1.75)`. **Enemy scaling**: `1.07^(floor-1)`.
- **Level-up**: full HP/resource, base stats up, **+1 skill point**.

---

## Combat — the full playbook

Round-based: you act, they act. The round header shows **HP bar, resource bar, belt, and
active bonuses** (`Adrenaline x1`); each monster shows a name + HP bar.

### Commands

| Input | Action |
| --- | --- |
| `a` | Basic attack (first enemy). Always connects. |
| `s` | Cast a spell — pick from the list (each shows cost, cooldown, effect). |
| `i` | Items — drink a potion from belt/inventory. |
| `1` / `2` | Drink the bound belt potion instantly. |
| `f` | Flee (next line: back to camp, nothing gained that fight). |
| `h` | Help. |

Opening any combat menu — potions, spells, help, or a belt slot that's empty — and
leaving it without acting costs **nothing**: the round does not advance and the enemy
does not attack. Only a real action (attack, spell cast, potion drunk, failed flee)
commits your turn.

### Spellcasting

- Spells cost **resource** (Stamina/Mana/Energy) and usually have a **cooldown**.
- Resource regenerates every turn (**+2 Mage, +1 others**, plus gear/runes/training).
- Element-typed hits use the triangle above; `Absolute Zero`, `Pyroclasm`, Fire/Frost/
  Arcane Mages match elements to the enemy's weakness.
- Statuses tick: **Burn / Bleed / Poison** (damage over time), **Slow**, **Stun**,
  **Armor Shred / DEF-down**, **Enfeeble** (enemy ATK down), **Vulnerable** (they take
  more damage).

### Enemy affixes (shown on the enemy row)

| Affix | Behavior |
| --- | --- |
| Vampiric | Lifesteals on its hits. |
| Regenerating | Heals HP each turn. |
| Armored | High defense — shred it or use spell damage. |
| Ethereal | Sometimes phases out of damage (chance to dodge). |
| Berserker | Rages — hits harder as it takes damage. |
| Cursed | Debuffs you when it acts. |

**Elites** (from floor 4, up to ~35% by mid-game) are rare-size normal monsters with
extra crit, extra HP, and extra XP. **Multi-monster packs** appear from floor 5 (45%).

### The boss lineup

```
Twin Fang · Baron Gore · Ashen Witch · Stone Tyrant · Void Serpent
Infernal Duke · Abyssal Warden · The Sunderer · Nightmare Sovereign · Eternal One
```

Named (and damage-dealing) from the deepest biome. A boss is the last encounter of every
5th floor — the `<BOSS FIGHT>` warning precedes it. Defeating one marks **Boss Slayer**
and feeds your vault records.

---

## Spells — full class trees

Learn spells at the **Trainer** (camp 6) from the spell tree. Each branch is a column of
ranked spells; enter `rank branch` — the two numbers shown on every row of the tree. E.g.
`1 1` = rank 1 of branch 1 (your opening spell), `2 1` = rank 2 of branch 1 (Vanish),
`2 2` = rank 2 of branch 2 (Corrosive Slash). Costs **skill points** (`rank`: 1, 2, 3, 4);
a rank is *locked* until the previous rank in the same branch is owned. `P` opens passives.
You start with 2 points and get 1 per level.

### Warrior

**Berserker**
| Rank | Spell | Cost | CD | Effect |
| --- | --- | --- | --- | --- |
| 1 | Cleave | 3 res | - | 3-4 dmg |
| 2 | Whirlwind | 7 res | 2 | 7-9 dmg x2 (hits twice) |
| 3 | Executioner | 12 res | 3 | 12-15 dmg + Bleed 3 turns |
| 4 | Bloodrage | 8 res | 4 | 20-25 dmg + Bleed, always |

**Defender**
| Rank | Spell | Cost | CD | Effect |
| --- | --- | --- | --- | --- |
| 1 | Shield Slam | 2 res | - | 2-3 dmg, +20 DEF 1 turn |
| 2 | Iron Skin | 3 res | 3 | +60% DEF for 2 turns (buff) |
| 3 | Bastion | 4 res | 3 | heal 10-11 HP +30% DEF 2 turns |
| 4 | Bulwark | 5 res | 5 | +100% DEF for 2 turns |

**Warcry**
| Rank | Spell | Cost | CD | Effect |
| --- | --- | --- | --- | --- |
| 1 | Demoralize | 1 res | - | 1-2 dmg, 40% to drop enemy ATK |
| 2 | Rally | 3 res | 3 | heal 8-9 HP +25% ATK 2 turns |
| 3 | War Horn | 4 res | 4 | +50% ATK for 3 turns (self) |
| 4 | Cripple | 3 res | 2 | Enfeeble 3 turns, enemy ATK -35% |

*(damage ranges scale with level via each spell's level multiplier — the table is at
level 1.)*

### Mage

**Fire**
| Rank | Spell | Cost | CD | Effect |
| --- | --- | --- | --- | --- |
| 1 | Firebolt | 4 res | - | 4-5 dmg + Burn 2 |
| 2 | Meteor | 9 res | - | 9-12 dmg + Burn 3 |
| 3 | Inferno | 16 res | - | 16-20 dmg + Burn 3 |
| 4 | Pyroclasm | 9 res | 4 | 22-28 dmg + Burn 3 |

**Frost**
| Rank | Spell | Cost | CD | Effect |
| --- | --- | --- | --- | --- |
| 1 | Frostbolt | 3 res | - | 3-4 dmg + Slow 1 |
| 2 | Ice Nova | 6 res | - | 6-8 dmg + Slow 2, 30% stun |
| 3 | Blizzard | 11 res | - | 11-14 dmg x2, Slow 2, 25% stun |
| 4 | Absolute Zero | 8 res | 4 | 14-18 dmg + Slow 3, 40% stun |

**Arcane**
| Rank | Spell | Cost | CD | Effect |
| --- | --- | --- | --- | --- |
| 1 | Arcane Missiles | 2 res | - | 2-3 dmg x3 |
| 2 | Arcane Barrage | 6 res | - | 6-8 dmg, 25% stun |
| 3 | Disintegrate | 14 res | - | 14-18 dmg, 40% stun |
| 4 | Hex | 6 res | 3 | Vulnerable 3 turns, enemy takes +30% |

*(All Mage spells are element-typed — Fire/Frost/Arcane branches respectively.)*

### Rogue

**Shadow**
| Rank | Spell | Cost | CD | Effect |
| --- | --- | --- | --- | --- |
| 1 | Backstab | 2 res | 1 | 4-5 dmg, +40% crit self |
| 2 | Vanish | 3 res | 4 | +40% DEF 1 turn + Guard |
| 3 | Shadow Step | 6 res | 3 | 9-12 dmg x2, +60% crit self |
| 4 | Shadow Dance | 8 res | 4 | 8-10 dmg x3, +100% crit self |

**Poison**
| Rank | Spell | Cost | CD | Effect |
| --- | --- | --- | --- | --- |
| 1 | Venom Blade | 2 res | - | 2-3 dmg + Poison 3 |
| 2 | Corrosive Slash | 3 res | - | 3-4 dmg + Poison 2, 60% shred |
| 3 | Plague Sting | 8 res | - | 8-10 dmg + Poison 4, 30% shred |
| 4 | Crippling Venom | 6 res | 3 | Poison 3, enemy ATK -30% |

**Tricks**
| Rank | Spell | Cost | CD | Effect |
| --- | --- | --- | --- | --- |
| 1 | Double Strike | 3 res | - | 3-4 dmg x2 |
| 2 | Perfect Evasion | 3 res | 4 | +60% DEF 2 turns + Guard |
| 3 | Lethal Flourish | 7 res | 3 | 10-13 dmg x2, +80% crit self |
| 4 | Masterstroke | 9 res | 4 | 9-12 dmg x3, +120% crit self |

Spell damage = `max(1, round(potency + lvlScale x level) x enemyScale(floor))`, where
`enemyScale(floor) = 1.07^(floor-1)` — the same curve enemies use for HP/attack. The
numbers above are your Floor-1, level-1 values; as you level *and* descend, everything
hits harder. Crit-self multipliers stack with class crit bonus — a Rogue's Shadow/Tricks
line is a crit machine.

---

## Passives & training

From the trainer's passive screen (`P`), permanent, 1 skill point per rank, max 5 ranks:

| Train | Effect per rank |
| --- | --- |
| Might | +3 ATK |
| Vitality | +10 max HP |
| Focus | +3 max resource |
| Tenacity | +1 DEF |
| Fleetness | +1 HP regen / turn |

**Respec** (camp 7) refunds all skill points for a gold fee (base ~40g, scales with
investment). The spell tree displayed shows exactly what refund you'll get.

---

## Gear — everything there is to know

### Rarity & how many affixes

| Rarity | Affixes | Sockets |
| --- | --- | --- |
| Common | 1 | 0 |
| Uncommon | 1 | 1 |
| Rare | 2 | 1 |
| Epic | 3 | 2 |
| Legendary | 4 | 3 |

### Tiers (when they start dropping)

| Tier | Floor |
| --- | --- |
| Iron | 1 |
| Steel | 10 |
| Mythril | 24 |
| Adamant | 45 |
| Void | 75 |

### What makes an item

- **Slots**: Weapon, Armor, Ring, Amulet, Helm, Gloves, Boots (weapons add ATK, defenses
  add DEF).
- **Power**: the flat stat of the slot — bigger is better.
- **Affixes** (the good stuff): Damage %, Crit Chance, Crit Bonus, Life Steal, Max HP,
  Defense, HP Regen, Mana, Mana Regen, XP Gain.
- **Naming**: each affix type pulls a flavor word that becomes the item's *name* —
  `Enlightened` = XP Gain, `Cunning` = Crit Chance, `Scholarly` = XP Gain,
  `Draining` = Life Steal, `Serene` = Mana Regen, etc. So `Enlightened Iron Ring` is an
  Iron-tier ring with an XP affix.

### Sets (2-piece / 3-piece from your equipped gear)

| Set | 2 pieces | 3 pieces |
| --- | --- | --- |
| Titanic | +15% max HP | +3 HP regen/turn |
| Infernal | +12% ATK | +20 crit damage |
| Frostbound | +10% max resource | +3 mana regen/turn |
| Voidwalk | +10% DEF | +3% life steal |

Set identity shows on the item (`[Set: Titanic]`); mix-and-match is legal but the
bonuses reward focusing one set.

### The blacksmith (camp 2)

| Action | Cost | What it does |
| --- | --- | --- |
| Upgrade | `5 x (tier+1) + iLvl/2` shards | Raise item power / rarity step. |
| Reforge | `25 x (rarity+1) x (1 + iLvl/8)` gold | Reroll the affix(es). |
| Socket | a runestone | Insert a rune (only if the item has a socket). |
| Awaken | `50 x (rarity+1) x (1 + iLvl/8)` essence | Item enchantment step up. |
| Extract | `10 + iLvl` gold | Pull a rune back out of an item. |

Early game: upgrade your best Rare for shards. Mid: reforge for the affix you want.
Late: socket + awaken Legendaries.

### Runes (runestones)

Flat bonuses dropped/looted from floor events and monsters. Sanity roll: Power (ATK %),
Vitality (HP %), Warding (DEF %), Flow (mana regen), Finesse (crit chance). Socketed
into gear; extractable. A rune's value scales with the floor it dropped on.

### Salvage & sell

- **Salvage**: `1 + iLvl/20 + 2 x rarity` shards (Epic+ also yields **1 essence**).
- **Sell**: `8 x iLvl x (rarity+1) / 2` gold.
- Both live in the merchant menu — keep the bags moving, bags are finite-ish.

### The merchant (camp 3)

Healing Draught (10g), Mana Draught (8g), Shards x3 (12g), Essence (30g), plus Sell and
Salvage. Potions are **floor-scaled**: the deeper you go, the bigger the heal. Same goes
for dropped potions.

### The belt (camp 5)

`bind slot 1` / `bind slot 2` (potions only), then `1` / `2` in combat to drink instantly.
The round header shows `Belt: [1] - [2] -`. Binds persist per-belt, and a bound potion
is consumed from inventory.

---

## The Rift — environs

### Biomes (every 5 floors, each with its own color + flavor line)

| Floors | Biome |
| --- | --- |
| 1-5 | Crystal Shallows |
| 6-10 | Mossfall Depths |
| 11-15 | Hollow Warrens |
| 16-20 | Cinder Vault |
| 21-25 | Frostbound Fissure |
| 26-30 | Drowned Crypt |
| 31-35 | Ash Cathedral |
| 36-40 | Mire of Echoes |
| 41-45 | Basalt Maw |
| 46-55 | Shattered Twilight |
| 56-69 | Abyssal Shrine |
| 70+ | The Endless Dark |

### Floor events (~28% per floor, before the fights)

| Event | Effect |
| --- | --- |
| A Glimmering Fountain | Restore 1/3 HP + half resource. |
| A Forgotten Cache | Gold (scales with floor). |
| A Rune-Fall | 1-2 runestones. |
| A Hidden Trap | Burst damage (1/4 max HP) or dodge. |
| A Traveling Apothecary | Buy a Healing Draught for 40g (or decline). |
| A Fading Shrine | 1 runestone + 1/5 max HP restored. |
| A Hexed Chest | 50/50: trap (damage + lost gold) or gold + runestone. |
| A Whispering Mural | XP + gold (scales with floor). |
| A Storm of Runes | 2 runestones + half resource. |
| A Puzzling Obelisk | Pick a rune: fortune (shards + heal) or misfortune (damage). |

Events are **risk/reward**; shrines and storms are pure upside, the cache/fountain/mural
are near-guaranteed upside, the chest and obelisk are gambles, and the trap is the Rift
collecting its toll.

---

## Progression layers (the long game)

- **Rift Mastery** (+1 at every 25th cleared floor): compounding raw power —
  ATK x1.08, max HP x1.06, max resource x1.05 per mastery tick.
- **Rift Aspects**: collect from grinding floors; each Ascension adds one.
- **Ascension** (Floor 100+, camp 11): +1 aspect, pick a **Rift Aspect perk**
  (Heirloom +20% gold, Runeforge more runestone drops, Insight +10% XP, Vitals +8% max HP,
  Leeching +2% life steal, Bulwark +6 DEF), reset to Floor 1, full heal, and the Rift
  grows stronger.
- **Glory (account-wide)**: 15 achievements persist across every run — First Blood,
  Boss Slayer, Deep Delver, Dungeon Master, Riftbreaker, Legendary Hunter,
  Death Is a Teacher, Master of the Rift, Aspect of Eternity, Slayer, Midas,
  Hardcore Heart, From the Ashes, Immortal, Pay the Iron Price. The **Records** screen
  (camp 10) shows best floor, bosses slain, kills, legendaries found, mastery, the n/15
  tracker, and the Fallen list.

---

## Death, save files, and the rules of the road

- **Softcore death**: lose 20% gold, slide back to the previous floor's camp. Never fatal.
- **Hardcore death**: `glory::fall()` — save erased, Fallen hero recorded (class, level,
  floor, kills, gold earned), "Pay the Iron Price" earned.
- Saves (`save1.rpg`..`save3.rpg`) and Glory (`glory.rpg`) are **checksummed**; the
  current save format is v9 (hardcore flag). Saves persist at each floor's milestones,
  at camp, and on clean quit.
- There is **no mid-run save-scumming** — deaths in hardcore are final by design.

---

## Strategy notes

- **Warrior**: take Cleave, then either push Berserker for raw damage or grab Iron Skin
  early for survivability on boss floors. Adrenaline loves a big single hit — time
  Cleave/Whirlwind after you've been hit twice.
- **Mage**: Mana runs out faster than HP in this game. Firebolt spam + Meteor when the
  triangle favors you; conversely respect Frost for stuns on every 5th floor. Arcane
  Missiles is the best cost-per-damage opener in the game.
- **Rogue**: First Strike makes your opening spell land *hard* — Backstab or Double
  Strike on a 1-shot opening, then poison stack the boss. Lethal/Masterstroke turn
  crit+1-shot windows into burst.
- **Money order**: potions when you need them, shards for upgrades on your best piece,
  essence for Legendaries, runestones collected (never sold).
- **Defense is a trap to over-invest in** — 100 DEF is halving damage, and bosses
  ignore it partially; HP and leech keep you in the fight longer.
- **Learn your first spell before Floor 5.** The boss at 5 will check whether you did.
- **Flee is honest**: a bad pack costs a floor, not your run. The Rift always lets you
  descend again.

---

## Appendix — exact formulas

| Thing | Formula |
| --- | --- |
| XP to next level | `ceil(52 x level^1.75)` |
| Enemy HP/ATK/DEF scale | `1.07^(floor-1)` per floor |
| Damage roll | `ATK x (0.9..1.1)` |
| Mitigation | `raw x 100 / (100 + DEF)` |
| Crit | `raw x (1.5 + critBonus/100)` |
| Element triangle | Fire > Frost > Arcane > Fire (x1.3 weak / x0.7 same) |
| Skill point cost per spell rank | `rank depth + 1` points |
| Rune value | type-based roll + floor scaling |
| Sell price | `8 x iLvl x (rarity+1) / 2` gold |
| Salvage | `1 + iLvl/20 + 2 x rarity` shards (Epic+ adds 1 essence) |
| Reforge | `25 x (rarity+1) x (1 + iLvl/8)` gold |
| Upgrade | `5 x (tier+1) + iLvl/2` shards |
| Awaken | `50 x (rarity+1) x (1 + iLvl/8)` essence |
| Extract | `10 + iLvl` gold |
| Rift Mastery | ATK x1.08, HP x1.06, resource x1.05 per point (compounding) |

---

*Enjoy the descent, and remember: the Rift keeps the bodies.*