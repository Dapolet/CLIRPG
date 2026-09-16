#include "combat.hpp"

#include "io.hpp"
#include "ui.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>

namespace rpg::combat {

const char* enemyAffixName(EnemyAffix a) {
    switch (a) {
        case EnemyAffix::Vampiric:     return "Vampiric";
        case EnemyAffix::Regenerating: return "Regenerating";
        case EnemyAffix::Armored:      return "Armored";
        case EnemyAffix::Ethereal:     return "Ethereal";
        case EnemyAffix::Berserker:    return "Berserker";
        case EnemyAffix::Cursed:       return "Cursed";
        default:                       return "";
    }
}

bool Enemy::has(EnemyAffix a) const {
    return std::find(affixes.begin(), affixes.end(), a) != affixes.end();
}

namespace {

struct Timer {
    core::StatusEffect eff = core::StatusEffect::None;
    int turns = 0;
    int power = 0;
};

struct EnemyBase {
    const char* name;
    int minFloor;
    int hp, atk, def, crit, xp;
    core::Element align;
};

constexpr std::array<EnemyBase, 18> kBestiary = {{
    { "Goblin",       1,  12,  5,  0,  5,  8, core::Element::None },
    { "Slime",        1,  14,  4,  0,  5,  7, core::Element::None },
    { "Skeleton",     2,  16,  6,  2,  5, 10, core::Element::None },
    { "Orc",          3,  22,  8,  2,  5, 12, core::Element::None },
    { "Cultist",      4,  14,  8,  0,  8, 13, core::Element::Arcane },
    { "Wraith",       6,  18, 10,  0, 10, 15, core::Element::Frost },
    { "Imp",          8,  12,  9,  0, 12, 16, core::Element::Fire },
    { "Golem",       10,  34,  9,  6,  5, 18, core::Element::None },
    { "Troll",       12,  40, 11,  3,  5, 20, core::Element::None },
    { "Demon",       16,  46, 14,  4,  8, 24, core::Element::None },
    { "Night Terror", 22,  60, 17,  6, 10, 30, core::Element::None },
    { "Dread Lord",   30,  78, 22,  8, 10, 38, core::Element::None },
    { "Wight Lord",   36,  95, 26, 10, 10, 45, core::Element::Frost },
    { "Magma Golem",  42, 115, 28, 12,  5, 48, core::Element::Fire },
    { "Void Caller",  50, 130, 32, 12, 10, 55, core::Element::Arcane },
    { "Bone Wraith",  55, 120, 30, 14, 12, 58, core::Element::None },
    { "Doom Herald",  65, 160, 38, 16, 12, 70, core::Element::None },
    { "Void Wraith",  78, 200, 44, 20, 14, 85, core::Element::Arcane },
}};

constexpr std::array<const char*, 10> kBossNames = {
    "Twin Fang", "Baron Gore", "Ashen Witch", "Stone Tyrant", "Void Serpent",
    "Infernal Duke", "Abyssal Warden", "The Sunderer", "Nightmare Sovereign", "Eternal One",
};

constexpr std::array<EnemyAffix, 6> kAffixPool = {
    EnemyAffix::Vampiric, EnemyAffix::Regenerating, EnemyAffix::Armored,
    EnemyAffix::Ethereal, EnemyAffix::Berserker,    EnemyAffix::Cursed,
};

constexpr std::array<core::Element, 3> kElements = {
    core::Element::Fire, core::Element::Frost, core::Element::Arcane,
};

// ---------------------------------------------------------------------------
// status helpers
// ---------------------------------------------------------------------------

bool hasStatus(const std::vector<Timer>& ts, core::StatusEffect e, int* power = nullptr) {
    for (const auto& t : ts) {
        if (t.eff == e) {
            if (power) *power = t.power;
            return true;
        }
    }
    return false;
}

void applyStatus(std::vector<Timer>& ts, core::StatusEffect e, int turns, int power) {
    for (auto& t : ts) {
        if (t.eff == e) {
            t.turns = std::max(t.turns, turns);
            t.power = std::max(t.power, power);
            return;
        }
    }
    ts.push_back(Timer{ e, turns, power });
}

bool isDot(const Timer& t) {
    return t.eff == core::StatusEffect::Burn || t.eff == core::StatusEffect::Bleed ||
           t.eff == core::StatusEffect::Poison;
}

// Decrement turn timers (not dots).
void tickTimers(std::vector<Timer>& ts) {
    std::erase_if(ts, [](Timer& t) {
        if (isDot(t)) return false;
        return --t.turns <= 0;
    });
}

std::string timerList(const std::vector<Timer>& ts) {
    std::string out;
    for (const auto& t : ts)
        out += std::string(core::statusName(t.eff)) + "(" + std::to_string(t.turns) + ") ";
    return out;
}

int enemyShred(const std::vector<Timer>& ts) {
    int p = 0;
    if (hasStatus(ts, core::StatusEffect::ArmorShred, &p)) return p;
    return 0;
}

int mitigatedDef(const Enemy& e, const std::vector<Timer>& eb) {
    return std::max(0, e.defense - enemyShred(eb));
}

void printEnemies(const std::vector<Enemy>& enemies) {
    for (std::size_t i = 0; i < enemies.size(); ++i) {
        const auto& e = enemies[i];
        if (!e.alive()) continue;
        std::string line = ui::chip(static_cast<int>(i + 1));
        line += ui::elementTag(e.align);
        if (e.align != core::Element::None) line += " ";
        line += ui::bold(e.name);
        if (e.elite) line += ui::color(ui::c::gold, "  *elite*");
        if (e.boss)  line += ui::color(ui::c::bad, ui::bold("  <BOSS>"));
        line += " HP " + ui::hpMeter(e.hp, e.hpMax, 10);
        line += ui::dim(" " + std::to_string(e.hp) + "/" + std::to_string(e.hpMax));
        if (e.defense > 0) line += ui::color(ui::c::soft, " DEF " + std::to_string(e.defense));
        for (const auto a : e.affixes)
            if (a != EnemyAffix::None) line += ui::color(ui::c::arcane, " " + std::string(enemyAffixName(a)));
        ui::panelLine(line);
    }
}

// ---------------------------------------------------------------------------
// generation
// ---------------------------------------------------------------------------

Enemy makeEnemyByName(const EnemyBase& b, int floor, bool elite, core::Rng& rng, int aspect) {
    const double sc = core::enemyScale(floor) * core::aspectScale(aspect);
    Enemy e;
    e.name = b.name;
    e.elite = elite;
    e.hpMax = e.hp = static_cast<int>(static_cast<double>(b.hp) * sc * (elite ? 2.0 : 1.0) *
                                      (0.9 + 0.2 * rng.roll01()));
    e.attack = static_cast<int>(static_cast<double>(b.atk) * sc * (elite ? 1.5 : 1.0));
    e.defense = b.def + floor / 4;
    e.critChance = b.crit + (elite ? 5 : 0);
    e.xpReward = static_cast<int>(static_cast<double>(b.xp) * std::ceil(sc) * (elite ? 2 : 1));
    e.goldReward = static_cast<int>(12.0 * floor * (0.8 + 0.4 * rng.roll01()) *
                                    core::aspectScale(aspect));
    e.align = b.align;
    if (e.align == core::Element::None && floor >= 6 && rng.chance(0.30))
        e.align = kElements[rng.pick(kElements.size())];

    EnemyAffix affix = EnemyAffix::None;
    if (elite) affix = kAffixPool[rng.pick(kAffixPool.size())];
    else if (floor >= 8 && rng.chance(0.18)) affix = kAffixPool[rng.pick(kAffixPool.size())];

    if (affix != EnemyAffix::None) e.affixes.push_back(affix);
    if (e.has(EnemyAffix::Armored)) e.defense *= 2;
    return e;
}

} // namespace

std::vector<Enemy> makeEncounter(int floor, core::Rng& rng, int aspect) {
    std::vector<const EnemyBase*> pool;
    for (const auto& b : kBestiary)
        if (b.minFloor <= floor) pool.push_back(&b);

    std::vector<Enemy> out;
    const std::size_t count = floor >= 5 && rng.chance(0.45) ? 2u : 1u;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const bool elite = floor > 3 && rng.chance(std::min(0.06 + 0.01 * floor, 0.35));
        out.push_back(makeEnemyByName(*pool[rng.pick(pool.size())], floor, elite, rng, aspect));
    }
    return out;
}

Enemy makeBoss(int floor, core::Rng& rng, int aspect) {
    const int idx = floor / 5;
    const double sc = core::enemyScale(floor) * core::aspectScale(aspect);
    Enemy e;
    e.name = kBossNames[static_cast<std::size_t>(idx % kBossNames.size())];
    e.boss = true;
    e.hpMax = e.hp = static_cast<int>(60.0 * sc * (1.0 + 0.1 * (idx % 10)) * (0.9 + 0.2 * rng.roll01()));
    e.attack = static_cast<int>(10.0 * sc * 1.6);
    e.defense = floor / 3;
    e.critChance = 15;
    e.xpReward = static_cast<int>(20.0 * std::ceil(sc) * 5.0);
    e.goldReward = static_cast<int>(60.0 * floor * (0.8 + 0.4 * rng.roll01()) *
                                    core::aspectScale(aspect));
    if (floor >= 6 && rng.chance(0.6)) e.align = kElements[rng.pick(kElements.size())];

    const int affixes = floor >= 15 ? 2 : 1;
    std::vector<int> used;
    for (int i = 0; i < affixes; ++i) {
        int pick;
        do {
            pick = static_cast<int>(rng.pick(kAffixPool.size()));
        } while (std::find(used.begin(), used.end(), pick) != used.end());
        used.push_back(pick);
        e.affixes.push_back(kAffixPool[static_cast<std::size_t>(pick)]);
    }
    if (e.has(EnemyAffix::Armored)) e.defense *= 2;
    return e;
}

int biomeIndexFor(int floor) {
    if (floor <= 5)   return 0;
    if (floor <= 10)  return 1;
    if (floor <= 15)  return 2;
    if (floor <= 20)  return 3;
    if (floor <= 25)  return 4;
    if (floor <= 30)  return 5;
    if (floor <= 35)  return 6;
    if (floor <= 40)  return 7;
    if (floor <= 45)  return 8;
    if (floor <= 55)  return 9;
    if (floor <= 69)  return 10;
    return 11;
}

const char* biomeFor(int floor) {
    static constexpr std::array<const char*, 12> kBiomes = {
        "Crystal Shallows",    // 1-5
        "Mossfall Depths",     // 6-10
        "Hollow Warrens",      // 11-15
        "Cinder Vault",        // 16-20
        "Frostbound Fissure",  // 21-25
        "Drowned Crypt",       // 26-30
        "Ash Cathedral",       // 31-35
        "Mire of Echoes",      // 36-40
        "Basalt Maw",          // 41-45
        "Shattered Twilight",  // 46-55
        "Abyssal Shrine",      // 56-69
        "The Endless Dark",    // 70+
    };
    return kBiomes[static_cast<std::size_t>(biomeIndexFor(floor))];
}

const char* biomeLoreFor(int floor) {
    static constexpr std::array<const char*, 12> kLore = {
        "Prismatic shards gleam in the shallows.",
        "Moss-draped roots creak overhead in the dark.",
        "Burrows chitter just beyond the torchlight.",
        "Embers hiss as they drift from the vault roof.",
        "Frozen chimes ring out across the fissure.",
        "Water drips through drowned, echoing halls.",
        "Ash sits in rows upon silent black pews.",
        "Will-o'-wisps dance over the rotting mire.",
        "Molten veins throb deep inside the basalt.",
        "The light bends strangely; shadows recede.",
        "A low chant hums from the black curtain.",
        "There is no floor beneath the Endless Dark.",
    };
    return kLore[static_cast<std::size_t>(biomeIndexFor(floor))];
}

// ---------------------------------------------------------------------------
// fight
// ---------------------------------------------------------------------------

namespace {

// One player basic attack. Handles rage, adrenaline, crit, ethereal, lifesteal,
// cursed thorns, armor shred. Returns damage dealt (0 = whiffed/missed).
// Helper: extra damage taken by enemy from Vulnerable (+%). 1.0 = none.
double vulnMult(const std::vector<Timer>& eb) {
    int pct = 0;
    if (hasStatus(eb, core::StatusEffect::Vulnerable, &pct))
        return static_cast<double>(100 + pct) / 100.0;
    return 1.0;
}
int dealAttack(Character& pc, const EffectiveStats& st, std::vector<Timer>& pb,
               Enemy& e, std::vector<Timer>& eb, core::Rng& rng,
               int& adrenaline, int critExtra) {
    double dmg = static_cast<double>(st.attack) * (0.9 + 0.2 * rng.roll01());
    int rage = 0;
    if (hasStatus(pb, core::StatusEffect::Rage, &rage))
        dmg *= (100.0 + static_cast<double>(rage)) / 100.0;
    if (adrenaline > 0) {
        dmg *= 1.0 + 0.15 * static_cast<double>(adrenaline);
        adrenaline = 0;
    }
    if (e.has(EnemyAffix::Ethereal) && rng.chance(0.25)) {
        std::cout << "  " << e.name << " phases through your blade!\n";
        return 0;
    }
    dmg = core::mitigate(dmg, mitigatedDef(e, eb));
    if (rng.chance(static_cast<double>(st.critChance + critExtra) / 100.0))
        dmg = core::critValue(dmg, st.critBonus);
    dmg *= vulnMult(eb);
    const int dealt = std::max(1, static_cast<int>(dmg));
    e.hp = std::max(0, e.hp - dealt);
    std::cout << ui::bold("  You strike ") << e.name
              << ui::color(ui::c::gold, " for " + std::to_string(dealt) + " damage") << ".\n";
    if (st.lifeStealPct > 0) {
        const int steal = dealt * st.lifeStealPct / 100;
        pc.healHp(steal, st.maxHp);
        std::cout << ui::color(ui::c::shine, "  (+" + std::to_string(steal) + " lifesteal)") << "\n";
    }
    if (e.has(EnemyAffix::Cursed)) pc.takeDamage(dealt * 35 / 100);
    return dealt;
}

// Shared spell damage application for one enemy (single hit loop).
int dealSpell(Character& pc, const EffectiveStats& st, std::vector<Timer>& pb,
              const Spell& s, Enemy& e, std::vector<Timer>& eb, core::Rng& rng,
              int& adrenaline, int critExtra) {
    const int ei = mitigatedDef(e, eb);
    const double element = core::elementMult(s.element, e.align);
    double base = static_cast<double>(spellDamage(s, pc.level())) *
                  (1.0 + static_cast<double>(st.attack) / 100.0) * element;
    int rage = 0;
    if (hasStatus(pb, core::StatusEffect::Rage, &rage))
        base *= (100.0 + static_cast<double>(rage)) / 100.0;
    if (adrenaline > 0) {
        base *= 1.0 + 0.15 * static_cast<double>(adrenaline);
        adrenaline = 0;
    }
    int total = 0;
    for (int h = 0; h < s.hits; ++h) {
        double dmg = base * (0.9 + 0.2 * rng.roll01());
        if (e.has(EnemyAffix::Ethereal) && rng.chance(0.25)) {
            std::cout << ui::color(ui::c::frost, "  " + e.name + " phases through the spell!") << "\n";
            continue;
        }
        dmg = core::mitigate(dmg, ei);
        dmg *= vulnMult(eb);
        if (rng.chance(static_cast<double>(st.critChance + s.critBonusSelf + critExtra) / 100.0))
            dmg = core::critValue(dmg, st.critBonus);
        const int dealt = std::max(1, static_cast<int>(dmg));
        total += dealt;
        e.hp = std::max(0, e.hp - dealt);
    }
    if (total > 0) {
        std::cout << ui::bold("  " + s.name);
        if (s.element != core::Element::None && e.align != core::Element::None) {
            if (element == 1.3)
                std::cout << ui::color(ui::c::gold, ui::bold(" CRUSHES the ")
                          + std::string(core::elementName(e.align)) + " foe");
            else if (element == 0.7)
                std::cout << ui::color(ui::c::frost, " fizzles against the "
                          + std::string(core::elementName(e.align)) + " foe");
        }
        std::cout << ui::color(ui::c::gold, " for " + std::to_string(total) + " damage") << ".\n";
        if (st.lifeStealPct > 0) pc.healHp(total * st.lifeStealPct / 100, st.maxHp);
        if (e.has(EnemyAffix::Cursed)) pc.takeDamage(total * 35 / 100);
    } else {
        std::cout << ui::dim("  The spell fizzles.") << "\n";
    }
    return total;
}

} // namespace

Result fight(Character& pc, Vault& vault, std::vector<Enemy> enemies, core::Rng& rng) {
    Result res;
    const EffectiveStats st = pc.stats(vault);
    std::vector<Timer> pb;                        // player buffs/timers
    std::vector<std::vector<Timer>> eb(enemies.size());
    std::array<int, 12> cd{};
    bool bossSummoned = false;
    int adrenaline = 0;
    int roundNum = 0;

    const auto anyAlive = [&enemies]() {
        for (const auto& e : enemies) if (e.alive()) return true;
        return false;
    };
    // Manual multi-target selection.
    const auto chooseTarget = [&enemies]() {
        std::vector<std::size_t> alive;
        for (std::size_t i = 0; i < enemies.size(); ++i)
            if (enemies[i].alive()) alive.push_back(i);
        if (alive.size() <= 1) return alive.empty() ? std::size_t(0) : alive[0];
        std::cout << ui::color(ui::c::soft, "  Targets:") << "\n";
        for (std::size_t k = 0; k < alive.size(); ++k) {
            const Enemy& e = enemies[alive[k]];
            std::cout << ui::color(ui::c::shine, "    [" + std::to_string(k + 1) + "] ") << e.name
                      << "  HP " << e.hp << "/" << e.hpMax << "\n";
        }
        const std::size_t pick = static_cast<std::size_t>(
            io::askInt("Strike which?", 1, static_cast<int>(alive.size())) - 1);
        return alive[pick];
    };

    while (pc.alive() && anyAlive()) {
        // first round: record which affixes we are up against (bestiary)
        if (roundNum == 0) {
            for (const auto& e : enemies)
                if (e.alive())
                    for (const auto a : e.affixes)
                        if (a != EnemyAffix::None)
                            vault.bestiary.addAffix(std::string(enemyAffixName(a)));
        }
        ++roundNum;
        const int critExtra = pc.classId() == ClassId::Rogue && roundNum == 1 ? 25 : 0;
        // -- start of round: enemy dots --
        for (std::size_t i = 0; i < enemies.size(); ++i) {
            if (!enemies[i].alive()) continue;
            for (auto it = eb[i].begin(); it != eb[i].end();) {
                if (!isDot(*it) || it->power <= 0) { ++it; continue; }
                enemies[i].hp = std::max(0, enemies[i].hp - it->power);
                std::cout << ui::color(ui::c::bad, "  " + std::string(core::statusName(it->eff))
                          + " bites " + enemies[i].name + " for " + std::to_string(it->power))
                          << ".\n";
                if (--it->turns <= 0) it = eb[i].erase(it);
                else ++it;
            }
        }

        ui::panelTop("Round " + std::to_string(roundNum) + "  ·  L"
                     + std::to_string(pc.level()), 36);
        ui::panelLine("HP " + ui::hpMeter(pc.hp(), st.maxHp, 16)
                      + ui::dim(" " + std::to_string(pc.hp()) + "/" + std::to_string(st.maxHp))
                      + "   " + std::string(resourceName(pc.classId())) + " "
                      + ui::meter(pc.resource(), st.maxResource, 12, ui::c::mana)
                      + ui::dim(" " + std::to_string(pc.resource()) + "/"
                                + std::to_string(st.maxResource)));
        {
            std::string beltStr = "Belt:";
            for (int s = 0; s < 2; ++s) {
                const PotionKind k = vault.beltKind(s);
                const int n = ui::potionCount(vault, k);
                beltStr += "  " + std::string(ui::chip(s + 1)) + " ";
                beltStr += n > 0 ? ui::color(ui::c::good, std::to_string(n) + "x")
                                 : ui::dim("-");
            }
            ui::panelLine(beltStr);
        }
        if (!timerList(pb).empty())
            ui::panelLine(ui::dim("Buffs: ") + timerList(pb));
        if (pc.classId() == ClassId::Warrior && adrenaline > 0)
            ui::panelLine(ui::color(ui::c::bad, "Adrenaline x" + std::to_string(adrenaline)));
        ui::panelBottom();
        printEnemies(enemies);

        // cooldown tick
        for (auto& c : cd) c = std::max(0, c - 1);

        bool acted = false;
        char kind = 'a';
        const std::string cmd = io::readLine(
            "[a]ttack [s]pell [i]tems [f]lee (h help) > ");
        if (cmd.empty()) {
            kind = 'a';
        } else if (cmd[0] == 'h' || cmd[0] == '?') {
            std::cout << ui::dim("  a = basic attack   s = cast a learned spell\n"
                      "  i = use a potion   f = flee (75%)\n"
                      "  1/2 = drink that belt potion   h = this help\n");
            continue;
        } else {
            kind = cmd[0];
        }

        if (kind == '1' || kind == '2') {
            const int slotNum = kind == '1' ? 0 : 1;
            const PotionKind bk = vault.beltKind(slotNum);
            const int vIdx = vault.beltIndex(slotNum);
            if (bk == PotionKind::None || vIdx < 0) {
                std::cout << ui::color(ui::c::shine, "  Belt slot " + std::to_string(slotNum + 1)
                          + " has nothing to drink.") << "\n";
            } else {
                const Item p = vault.items[static_cast<std::size_t>(vIdx)];
                const int h = potionHeal(p, pc.currentFloor());
                const int m = potionMana(p, pc.currentFloor());
                if (h > 0) {
                    pc.healHp(h, st.maxHp);
                    std::cout << "  Restored " << h << " HP.\n";
                }
                if (m > 0) {
                    pc.restoreResource(m, st.maxResource);
                    std::cout << "  Restored " << m << " "
                              << resourceName(pc.classId()) << ".\n";
                }
                vault.usePotion(vIdx);
                acted = true;
            }
        } else if (kind == 'f') {
            if (rng.chance(0.75)) {
                std::cout << ui::color(ui::c::gold, "You slip away from the fight.") << "\n";
                res.fled = true;
                res.won = false;
                return res;
            }
            std::cout << ui::color(ui::c::bad, "They block your escape! You ready yourself.") << "\n";
            acted = true;
        } else if (kind == 'i') {
            struct PotOpt {
                int idx;
                bool belt;
            };
            std::vector<PotOpt> pots;
            const auto addPot = [&](int idx, bool belt) {
                if (idx >= 0 && vault.hasItem(idx) &&
                    vault.items[static_cast<std::size_t>(idx)].isPot())
                    pots.push_back({ idx, belt });
            };
            addPot(vault.beltIndex(0), true);
            addPot(vault.beltIndex(1), true);
            for (std::size_t i = 0; i < vault.items.size(); ++i) {
                if (!vault.items[i].isPot()) continue;
                if (std::find_if(pots.begin(), pots.end(),
                                 [&](const PotOpt& p) {
                                     return p.idx == static_cast<int>(i);
                                 }) != pots.end())
                    continue;
                pots.push_back({ static_cast<int>(i), false });
            }
            if (pots.empty()) {
                std::cout << ui::color(ui::c::shine, "  No potions in the bag.") << "\n";
            } else {
                std::cout << ui::color(ui::c::soft, "  Potions:") << "\n";
                for (std::size_t k = 0; k < pots.size(); ++k) {
                    const Item& p = vault.items[static_cast<std::size_t>(pots[k].idx)];
                    std::cout << ui::color(ui::c::shine, ui::chip(static_cast<int>(k + 1)))
                              << (pots[k].belt ? ui::color(ui::c::gold, ui::bold("(belt) ")) : "")
                              << ui::color(ui::rarityColor(p.rarity),
                                           p.describe(pc.currentFloor()))
                              << "\n";
                }
                std::cout << ui::dim(ui::chip(0) + "back") << "\n";
                const int pick = io::askInt("Use which? (0 back)", 0,
                                            static_cast<int>(pots.size()));
                if (pick > 0) {
                    const int vIdx = pots[static_cast<std::size_t>(pick - 1)].idx;
                    const Item p = vault.items[static_cast<std::size_t>(vIdx)];
                    const int h = potionHeal(p, pc.currentFloor());
                    const int m = potionMana(p, pc.currentFloor());
                    if (h > 0) {
                        pc.healHp(h, st.maxHp);
                        std::cout << "  Restored " << h << " HP.\n";
                    }
                    if (m > 0) {
                        pc.restoreResource(m, st.maxResource);
                        std::cout << "  Restored " << m << " "
                                  << resourceName(pc.classId()) << ".\n";
                    }
                    vault.usePotion(vIdx);
                    acted = true;
                }
            }
        } else if (kind == 's') {
            const auto& spells = classSpells(pc.classId());
            const auto tree = pc.tree();
            std::vector<int> learned;
            for (int i = 0; i < 12; ++i)
                if (tree[static_cast<std::size_t>(i)]) learned.push_back(i);
            if (learned.empty()) {
                std::cout << ui::dim("  You haven't learned any spells yet.") << "\n";
            } else {
                std::cout << ui::color(ui::c::soft, "  Spells:") << "\n";
                for (std::size_t k = 0; k < learned.size(); ++k) {
                    const Spell& s = spells[static_cast<std::size_t>(learned[k])];
                    std::cout << ui::color(ui::c::shine, ui::chip(static_cast<int>(k + 1)))
                              << ui::bold(s.name)
                              << ui::dim("  " + spellBlurb(s, pc.level())
                                         + "  (cost " + std::to_string(s.cost)
                                         + ", cd " + std::to_string(s.cooldown) + ")");
                    if (cd[static_cast<std::size_t>(learned[k])] > 0)
                        std::cout << ui::color(ui::c::bad, "  [recharging "
                                  + std::to_string(cd[static_cast<std::size_t>(learned[k])])
                                  + "]");
                    std::cout << "\n";
                }
                std::cout << ui::dim(ui::chip(0) + "back") << "\n";
                const int pick = io::askInt("Cast which? (0 back)", 0,
                                            static_cast<int>(learned.size()));
                if (pick > 0) {
                    const int idx = learned[static_cast<std::size_t>(pick - 1)];
                    if (cd[static_cast<std::size_t>(idx)] > 0) {
                        std::cout << ui::color(ui::c::bad, "  Still recharging!") << "\n";
                    } else if (!pc.canCast(spells[static_cast<std::size_t>(idx)])) {
                        std::cout << ui::color(ui::c::bad, "  Not enough "
                                  + std::string(resourceName(pc.classId())) + ".") << "\n";
                    } else {
                        const Spell& s = spells[static_cast<std::size_t>(idx)];
                        pc.spendResource(s.cost);
                        cd[static_cast<std::size_t>(idx)] = s.cooldown;

                        if (s.type == SpellType::Heal) {
                            const int h = spellHeal(s, pc.level());
                            pc.healHp(h, st.maxHp);
                            std::cout << ui::color(ui::c::good, ui::bold("  " + s.name
                                      + " restores " + std::to_string(h) + " HP.")) << "\n";
                        }
                        if (s.buffAttack > 0 && s.buffTurns > 0)
                            applyStatus(pb, core::StatusEffect::Rage, s.buffTurns, s.buffAttack);
                        if (s.buffDefense > 0 && s.buffTurns > 0)
                            applyStatus(pb, core::StatusEffect::Guard, s.buffTurns, s.buffDefense);
                        if (s.effect == core::StatusEffect::Guard)
                            applyStatus(pb, core::StatusEffect::Guard, s.effectTurns, 50);
                        if (s.effect == core::StatusEffect::Regeneration)
                            applyStatus(pb, core::StatusEffect::Regeneration, s.effectTurns,
                                        std::max(1, st.maxHp / 12));

                        if (s.potency > 0) {
                            const std::size_t tgt = chooseTarget();
                            Enemy& e = enemies[tgt];
                            const int total = dealSpell(pc, st, pb, s, e, eb[tgt], rng,
                                                        adrenaline, critExtra);
                            if (e.alive()) {
                                if (s.effect == core::StatusEffect::Burn ||
                                    s.effect == core::StatusEffect::Bleed ||
                                    s.effect == core::StatusEffect::Poison)
                                    applyStatus(eb[tgt], s.effect, s.effectTurns,
                                                std::max(2, total / 6));
                                if (s.effect == core::StatusEffect::Slow)
                                    applyStatus(eb[tgt], core::StatusEffect::Slow, s.effectTurns, 0);
                                if (s.armorShred > 0)
                                    applyStatus(eb[tgt], core::StatusEffect::ArmorShred, 3, s.armorShred);
                                if (s.stunChancePct > 0 && rng.chance(s.stunChancePct / 100.0))
                                    applyStatus(eb[tgt], core::StatusEffect::Stun, 1, 0);
                                if (s.enemyVulnPct > 0)
                                    applyStatus(eb[tgt], core::StatusEffect::Vulnerable, s.effectTurns, s.enemyVulnPct);
                                if (s.enemyAtkDownPct > 0)
                                    applyStatus(eb[tgt], core::StatusEffect::Enfeeble, s.effectTurns, s.enemyAtkDownPct);
                            }
                        } else {
                            std::cout << ui::color(ui::c::shine, "  " + s.name + "!") << "\n";
                        }
                        acted = true;
                    }
                }
            }
        } else {  // basic attack
            const std::size_t tgt = chooseTarget();
            Enemy& e = enemies[tgt];
            const int dealt = dealAttack(pc, st, pb, e, eb[tgt], rng,
                                         adrenaline, critExtra);
            if (dealt > 0) acted = true;
        }
        (void)acted;

        if (!pc.alive() || !anyAlive()) continue;

        // -- enemy phase (rogue First Strike: enemies skip round 1) --
        const bool rogueFree = pc.classId() == ClassId::Rogue && roundNum == 1;
        if (rogueFree) {
            std::cout << ui::color(ui::c::gold, "  First Strike! The enemies are caught off guard.") << "\n";
        } else {
            const std::size_t roster = enemies.size();
            for (std::size_t i = 0; i < roster; ++i) {
                if (!enemies[i].alive()) continue;

                if (hasStatus(eb[i], core::StatusEffect::Stun)) {
                    std::cout << ui::color(ui::c::frost, "  " + enemies[i].name + " is stunned and skips its turn.") << "\n";
                    tickTimers(eb[i]);
                    continue;
                }

                if (enemies[i].boss && !bossSummoned && enemies[i].hp <= enemies[i].hpMax / 2) {
                    bossSummoned = true;
                    std::cout << ui::color(ui::c::arcane, "  " + enemies[i].name + " summons a Dark Thrall!") << "\n";
                    const Enemy& src = enemies[i];
                    Enemy thrall;
                    thrall.name = "Dark Thrall";
                    thrall.hpMax = thrall.hp = std::max(4, src.hpMax / 4);
                    thrall.attack = std::max(2, src.attack / 2);
                    thrall.defense = src.defense / 2;
                    enemies.push_back(thrall);
                    eb.emplace_back();
                }

                Enemy& e = enemies[i];
                if (e.boss && e.hp <= e.hpMax / 4)
                    std::cout << ui::color(ui::c::bad, ui::bold("  " + e.name + " ENRAGES!")) << "\n";

                const bool enraged = e.boss && e.hp <= e.hpMax / 4;
                double atk = static_cast<double>(e.attack) * (0.9 + 0.2 * rng.roll01());
                if (hasStatus(eb[i], core::StatusEffect::Slow)) atk *= 0.5;
                int enfPct = 0;
                if (hasStatus(eb[i], core::StatusEffect::Enfeeble, &enfPct))
                    atk *= static_cast<double>(100 - enfPct) / 100.0;
                if (enraged) atk *= 2.0;
                if (e.has(EnemyAffix::Berserker))
                    atk *= 1.0 + 1.5 * (1.0 - static_cast<double>(e.hp) / e.hpMax);

                atk = core::mitigate(atk, st.defense);
                int guard = 0;
                if (hasStatus(pb, core::StatusEffect::Guard, &guard))
                    atk *= (100.0 - static_cast<double>(guard)) / 100.0;
                const int cap = std::max(1, st.maxHp * 60 / 100);
                const int dealt = std::clamp(static_cast<int>(atk), 1, cap);
                pc.takeDamage(dealt);
                std::cout << ui::color(ui::c::bad, "  " + e.name + " hits you for " + std::to_string(dealt)) << ".\n";

                if (pc.classId() == ClassId::Warrior && pc.alive() && adrenaline < 3)
                    ++adrenaline;
                if (e.has(EnemyAffix::Vampiric))
                    e.hp = std::min(e.hpMax, e.hp + dealt * 30 / 100);
                if (e.has(EnemyAffix::Regenerating)) {
                    const int reg = std::max(1, e.hpMax / 15);
                    e.hp = std::min(e.hpMax, e.hp + reg);
                    std::cout << ui::color(ui::c::good, "  " + e.name + " regenerates " + std::to_string(reg) + " HP.") << "\n";
                }

                tickTimers(eb[i]);
            }
        }

        // -- end of round: player regen, buff decay --
        if (hasStatus(pb, core::StatusEffect::Regeneration)) {
            int p = 0;
            hasStatus(pb, core::StatusEffect::Regeneration, &p);
            pc.healHp(p, st.maxHp);
        }
        pc.restoreResource(resourceRegenPerTurn(pc.classId()) + st.manaRegenPerTurn,
                           st.maxResource);
        tickTimers(pb);
    }

    res.won = pc.alive();
    if (res.won) {
        for (const auto& e : enemies) if (e.alive()) res.won = false;
    }
    for (const auto& e : enemies)
        if (!e.alive()) {
            ++res.kills;
            vault.bestiary.addEnemy(e.name);
            if (e.boss) vault.bestiary.addBoss(e.name);
        }

    if (res.won) {
        bool boss = false;
        int xp = 0;
        for (const auto& e : enemies) {
            xp += e.xpReward;
            if (e.boss) boss = true;
        }
        res.xp = xp;
        for (const auto& e : enemies) res.loot.gold += e.goldReward;
        res.loot.shards = (rng.chance(0.35) ? 1 : 0) + (rng.chance(0.10) ? 1 : 0);
        res.loot.essence = rng.chance(0.05) ? 1 : 0;
        res.loot.items = rollLoot(pc.currentFloor(), boss, rng);
        res.loot.runes = rollRunestoneLoot(pc.currentFloor(), boss, rng,
                                           vault.perks[static_cast<std::size_t>(PerkId::Runeforge)]);
        if (boss) {
            res.loot.shards += 2;
            res.loot.essence += 1;
        }
    }
    return res;
}

} // namespace rpg::combat