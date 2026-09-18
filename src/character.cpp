#include "character.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace rpg {

namespace {

constexpr int kMasteryCap = 150;

Spell atk(std::string n, int cost, int cd, int pot, double lvl, int hits = 1,
          core::StatusEffect eff = core::StatusEffect::None, int turns = 0,
          int shred = 0, int stun = 0) {
    return Spell{ .name = std::move(n), .cost = cost, .cooldown = cd,
                  .potency = pot, .lvlScale = lvl, .hits = hits, .effect = eff,
                  .effectTurns = turns, .armorShred = shred, .stunChancePct = stun };
}

std::vector<Spell> buildSpells(ClassId c) {
    using C = core::StatusEffect;
    std::vector<Spell> s;
    s.reserve(12);
    switch (c) {
        case ClassId::Warrior:
            // branch 0: Berserker
            s.push_back(atk("Cleave", 2, 1, 3, 1.0));
            s.push_back(atk("Whirlwind", 4, 2, 7, 2.0, 2));
            s.push_back(atk("Executioner", 6, 3, 12, 3.0, 1, C::Bleed, 3, 20));
            s.push_back(Spell{ .name = "Bloodrage", .cost = 8, .cooldown = 4,
                               .potency = 20, .lvlScale = 5.0, .effect = C::Bleed,
                               .effectTurns = 3 });
            // branch 1: Defender
            s.push_back(atk("Shield Slam", 2, 1, 2, 1.0, 1, C::None, 0, 30, 20));
            s.push_back(Spell{ .name = "Iron Skin", .type = SpellType::BuffSelf, .cost = 3,
                               .cooldown = 3, .buffDefense = 60, .buffTurns = 2 });
            s.push_back(Spell{ .name = "Bastion", .type = SpellType::Heal, .cost = 4,
                               .cooldown = 3, .lvlScale = 1.2, .healPower = 10,
                               .buffDefense = 30, .buffTurns = 2 });
            s.push_back(Spell{ .name = "Bulwark", .type = SpellType::BuffSelf, .cost = 5,
                               .cooldown = 5, .buffDefense = 100, .buffTurns = 2 });
            // branch 2: Warcry
            s.push_back(atk("Demoralize", 3, 2, 1, 1.0, 1, C::None, 0, 40, 10));
            s.push_back(Spell{ .name = "Rally", .type = SpellType::Heal, .cost = 3,
                               .cooldown = 3, .lvlScale = 1.0, .healPower = 8,
                               .buffAttack = 25, .buffTurns = 2 });
            s.push_back(Spell{ .name = "War Horn", .type = SpellType::BuffSelf, .cost = 4,
                               .cooldown = 4, .buffAttack = 50, .buffTurns = 3 });
            s.push_back(Spell{ .name = "Cripple", .cost = 3, .cooldown = 2, .potency = 2,
                               .lvlScale = 0.5, .effect = C::Enfeeble, .effectTurns = 3,
                               .enemyAtkDownPct = 35 });
            break;
        case ClassId::Mage:
            // branch 0: Fire
            s.push_back(atk("Firebolt", 2, 1, 4, 1.2, 1, C::Burn, 2));
            s.push_back(atk("Meteor", 4, 2, 9, 2.5, 1, C::Burn, 3));
            s.push_back(atk("Inferno", 7, 3, 16, 4.0, 1, C::Burn, 3));
            s.push_back(Spell{ .name = "Pyroclasm", .cost = 9, .cooldown = 4,
                               .potency = 22, .lvlScale = 5.5, .effect = C::Burn,
                               .effectTurns = 3 });
            // branch 1: Frost
            s.push_back(atk("Frostbolt", 2, 1, 3, 1.0, 1, C::Slow, 1));
            s.push_back(atk("Ice Nova", 4, 2, 6, 1.8, 1, C::Slow, 2, 0, 30));
            s.push_back(atk("Blizzard", 6, 3, 11, 3.0, 2, C::Slow, 2, 0, 25));
            s.push_back(Spell{ .name = "Absolute Zero", .cost = 8, .cooldown = 4,
                               .potency = 14, .lvlScale = 3.6, .effect = C::Slow,
                               .effectTurns = 3, .stunChancePct = 40 });
            // branch 2: Arcane
            s.push_back(atk("Arcane Missiles", 3, 1, 2, 0.9, 3));
            s.push_back(atk("Arcane Barrage", 5, 2, 6, 1.8, 1, C::None, 0, 25));
            s.push_back(atk("Disintegrate", 8, 4, 14, 3.6, 1, C::None, 0, 40));
            s.push_back(Spell{ .name = "Hex", .cost = 6, .cooldown = 3, .potency = 6,
                               .lvlScale = 1.5, .effect = C::Vulnerable, .effectTurns = 3,
                               .enemyVulnPct = 30 });
            for (int i = 0; i < 4; ++i) s[static_cast<std::size_t>(i)].element = core::Element::Fire;
            for (int i = 4; i < 8; ++i) s[static_cast<std::size_t>(i)].element = core::Element::Frost;
            for (int i = 8; i < 12; ++i) s[static_cast<std::size_t>(i)].element = core::Element::Arcane;
            break;
        case ClassId::Rogue:
            // branch 0: Shadow
            s.push_back(Spell{ .name = "Backstab", .cost = 2, .cooldown = 1,
                               .potency = 4, .lvlScale = 1.3, .critBonusSelf = 40 });
            s.push_back(Spell{ .name = "Vanish", .type = SpellType::BuffSelf, .cost = 3,
                               .cooldown = 4, .buffDefense = 40, .buffTurns = 1,
                               .effect = C::Guard, .effectTurns = 1 });
            s.push_back(Spell{ .name = "Shadow Step", .cost = 6, .cooldown = 3,
                               .potency = 9, .lvlScale = 2.5, .hits = 2, .critBonusSelf = 60 });
            s.push_back(Spell{ .name = "Shadow Dance", .cost = 8, .cooldown = 4,
                               .potency = 8, .lvlScale = 2.2, .hits = 3,
                               .critBonusSelf = 100 });
            // branch 1: Poison
            s.push_back(atk("Venom Blade", 2, 1, 2, 0.8, 1, C::Poison, 3));
            s.push_back(atk("Corrosive Slash", 4, 2, 3, 1.2, 1, C::Poison, 2, 60));
            s.push_back(atk("Plague Sting", 6, 3, 8, 2.0, 1, C::Poison, 4, 30));
            s.push_back(Spell{ .name = "Crippling Venom", .cost = 6, .cooldown = 3,
                               .potency = 3, .lvlScale = 0.8, .effect = C::Poison,
                               .effectTurns = 3, .enemyAtkDownPct = 30 });
            // branch 2: Tricks
            s.push_back(atk("Double Strike", 3, 2, 3, 1.0, 2));
            s.push_back(Spell{ .name = "Perfect Evasion", .type = SpellType::BuffSelf, .cost = 3,
                               .cooldown = 4, .buffDefense = 60, .buffTurns = 2,
                               .effect = C::Guard, .effectTurns = 2 });
            s.push_back(Spell{ .name = "Lethal Flourish", .cost = 7, .cooldown = 3,
                               .potency = 10, .lvlScale = 2.8, .hits = 2, .critBonusSelf = 80 });
            s.push_back(Spell{ .name = "Masterstroke", .cost = 9, .cooldown = 4,
                               .potency = 9, .lvlScale = 2.5, .hits = 3,
                               .critBonusSelf = 120 });
            break;
        case ClassId::Paladin:
            // branch 0: Holy
            s.push_back(atk("Smite", 2, 1, 3, 1.0));
            s.push_back(atk("Holy Lance", 4, 2, 7, 2.0, 1, C::None, 0, 20));
            s.push_back(atk("Judgement", 6, 3, 12, 3.0, 1, C::None, 0, 0, 20));
            s.push_back(Spell{ .name = "Divine Wrath", .cost = 8, .cooldown = 4,
                               .potency = 20, .lvlScale = 5.0, .effect = C::Vulnerable,
                               .effectTurns = 3, .enemyVulnPct = 30 });
            // branch 1: Order
            s.push_back(Spell{ .name = "Mend", .type = SpellType::Heal, .cost = 2,
                               .cooldown = 1, .lvlScale = 1.0, .healPower = 8 });
            s.push_back(Spell{ .name = "Aegis", .type = SpellType::BuffSelf, .cost = 3,
                               .cooldown = 3, .buffDefense = 50, .buffTurns = 2 });
            s.push_back(Spell{ .name = "Sanctuary", .type = SpellType::Heal, .cost = 4,
                               .cooldown = 3, .lvlScale = 1.4, .healPower = 12,
                               .buffDefense = 30, .buffTurns = 2 });
            s.push_back(Spell{ .name = "Consecrate", .type = SpellType::BuffSelf, .cost = 5,
                               .cooldown = 5, .buffDefense = 100, .buffTurns = 2 });
            // branch 2: Light
            s.push_back(Spell{ .name = "Rebuke", .cost = 3, .cooldown = 2, .potency = 1,
                               .lvlScale = 1.0, .effect = C::Enfeeble, .effectTurns = 3,
                               .enemyAtkDownPct = 30 });
            s.push_back(Spell{ .name = "Blessed Rally", .type = SpellType::Heal, .cost = 3,
                               .cooldown = 3, .lvlScale = 1.0, .healPower = 8,
                               .buffAttack = 25, .buffTurns = 2 });
            s.push_back(Spell{ .name = "Radiant Brand", .cost = 5, .cooldown = 3, .potency = 6,
                               .lvlScale = 1.6, .effect = C::Vulnerable, .effectTurns = 3,
                               .enemyVulnPct = 30 });
            s.push_back(atk("Sunburst", 8, 4, 14, 3.6, 1, C::Stun, 1, 0, 35));
            break;
        case ClassId::Necromancer:
            // branch 0: Decay
            s.push_back(atk("Corpse Rot", 2, 1, 4, 1.2, 1, C::Poison, 3));
            s.push_back(atk("Rustbrand", 4, 2, 7, 1.8, 1, C::Poison, 3, 30));
            s.push_back(atk("Virulence", 6, 3, 10, 2.6, 1, C::Poison, 4));
            s.push_back(atk("Flesh Harvest", 8, 4, 16, 4.5, 1, C::Poison, 3));
            // branch 1: Death
            s.push_back(atk("Death Bolt", 2, 1, 4, 1.3));
            s.push_back(atk("Bone Lance", 4, 2, 8, 2.0, 1, C::None, 0, 20));
            s.push_back(Spell{ .name = "Spirit Hex", .cost = 5, .cooldown = 3, .potency = 4,
                               .lvlScale = 1.2, .effect = C::Enfeeble, .effectTurns = 3,
                               .enemyAtkDownPct = 30 });
            s.push_back(Spell{ .name = "Death Mark", .cost = 7, .cooldown = 3, .potency = 12,
                               .lvlScale = 3.2, .effect = C::Vulnerable, .effectTurns = 3,
                               .enemyVulnPct = 30 });
            // branch 2: Undeath
            s.push_back(Spell{ .name = "Bone Armor", .type = SpellType::BuffSelf, .cost = 3,
                               .cooldown = 3, .buffDefense = 50, .buffTurns = 2 });
            s.push_back(Spell{ .name = "Soul Shroud", .type = SpellType::BuffSelf, .cost = 3,
                               .cooldown = 4, .buffDefense = 40, .buffTurns = 1,
                               .effect = C::Guard, .effectTurns = 1 });
            s.push_back(Spell{ .name = "Arise, Skeleton Knight", .type = SpellType::Summon,
                               .cost = 7, .cooldown = 6,
                               .summonHpPct = 50, .summonAtkPct = 60, .summonTurns = 0 });
            s.push_back(Spell{ .name = "Grave Pact", .type = SpellType::Heal, .cost = 6,
                               .cooldown = 5, .lvlScale = 1.8, .healPower = 14,
                               .buffDefense = 40, .buffTurns = 2 });
            break;
    }
    return s;
}

} // namespace

const std::vector<Spell>& classSpells(ClassId c) {
    static const std::vector<Spell> warrior = buildSpells(ClassId::Warrior);
    static const std::vector<Spell> mage    = buildSpells(ClassId::Mage);
    static const std::vector<Spell> rogue   = buildSpells(ClassId::Rogue);
    static const std::vector<Spell> paladin = buildSpells(ClassId::Paladin);
    static const std::vector<Spell> necro   = buildSpells(ClassId::Necromancer);
    switch (c) {
        case ClassId::Warrior: return warrior;
        case ClassId::Mage:    return mage;
        case ClassId::Rogue:   return rogue;
        case ClassId::Paladin: return paladin;
        case ClassId::Necromancer: return necro;
    }
    return warrior;
}

const char* className(ClassId c) {
    switch (c) {
        case ClassId::Warrior: return "Warrior";
        case ClassId::Mage:    return "Mage";
        case ClassId::Rogue:   return "Rogue";
        case ClassId::Paladin: return "Paladin";
        case ClassId::Necromancer: return "Necromancer";
    }
    return "?";
}

const char* resourceName(ClassId c) {
    switch (c) {
        case ClassId::Warrior: return "Stamina";
        case ClassId::Mage:    return "Mana";
        case ClassId::Rogue:   return "Energy";
        case ClassId::Paladin: return "Conviction";
        case ClassId::Necromancer: return "Soul";
    }
    return "?";
}

int spellDamage(const Spell& s, int level, int floor) {
    const double base = std::round(static_cast<double>(s.potency) +
                                   s.lvlScale * static_cast<double>(level));
    return std::max(1, static_cast<int>(base * core::enemyScale(floor)));
}

int spellHeal(const Spell& s, int level, int floor) {
    const int base = std::max(1, s.healPower + static_cast<int>(s.lvlScale * static_cast<double>(level)));
    return std::max(1, static_cast<int>(base * core::enemyScale(floor)));
}

int resourceRegenPerTurn(ClassId c) {
    return c == ClassId::Mage ? 2 : 1;
}

std::string spellBlurb(const Spell& s, int level, int floor, int attack) {
    std::string out;
    if (s.type == SpellType::Heal) {
        out += "heals " + std::to_string(spellHeal(s, level, floor)) + " HP";
        if (s.buffAttack > 0)  out += ", +" + std::to_string(s.buffAttack) + "% ATK";
        if (s.buffDefense > 0) out += ", +" + std::to_string(s.buffDefense) + "% DEF";
        if (s.buffTurns > 0)   out += " " + std::to_string(s.buffTurns) + " turns";
    } else if (s.type == SpellType::BuffSelf) {
        out += "self buff";
        if (s.buffAttack > 0)  out += " +" + std::to_string(s.buffAttack) + "% ATK";
        if (s.buffDefense > 0) out += " +" + std::to_string(s.buffDefense) + "% DEF";
        if (s.buffTurns > 0)   out += " " + std::to_string(s.buffTurns) + " turns";
    } else if (s.type == SpellType::Summon) {
        out += "summons a Skeleton Knight ("
               + std::to_string(s.summonHpPct) + "% HP, " + std::to_string(s.summonAtkPct)
               + "% ATK), bodyguards you";
    } else {
        if (s.element != core::Element::None)
            out += std::string(core::elementName(s.element)) + " ";
        int dmg = spellDamage(s, level, floor);
        dmg = static_cast<int>(std::lround(static_cast<double>(dmg)
                                           * (1.0 + static_cast<double>(attack) / 100.0)));
        out += std::to_string(dmg) + " dmg";
        if (s.hits > 1) out += " x" + std::to_string(s.hits);
        if (s.armorShred > 0)  out += ", shreds " + std::to_string(s.armorShred) + " DEF";
        if (s.stunChancePct > 0) out += ", " + std::to_string(s.stunChancePct) + "% stun";
        if (s.enemyAtkDownPct > 0)
            out += ", Enfeeble " + std::to_string(s.enemyAtkDownPct) + "%";
        if (s.enemyVulnPct > 0)
            out += ", Vulnerable " + std::to_string(s.enemyVulnPct) + "%";
        if (s.effect != core::StatusEffect::None) {
            out += ", " + std::string(core::statusName(s.effect));
            if (s.effectTurns > 0)
                out += " " + std::to_string(s.effectTurns) + " turns";
        }
    }
    out += ", " + std::to_string(s.cost) + " res, cd " + std::to_string(s.cooldown);
    return out;
}

const char* trainName(TrainId t) {
    switch (t) {
        case TrainId::Might:     return "Might";
        case TrainId::Vitality:  return "Vitality";
        case TrainId::Focus:     return "Focus";
        case TrainId::Tenacity:  return "Tenacity";
        case TrainId::Fleetness: return "Fleetness";
        default:                 return "?";
    }
}

const char* trainDesc(TrainId t) {
    switch (t) {
        case TrainId::Might:     return "+3 ATK per rank";
        case TrainId::Vitality:  return "+10 max HP per rank";
        case TrainId::Focus:     return "+3 max resource per rank";
        case TrainId::Tenacity:  return "+1 defense per rank";
        case TrainId::Fleetness: return "+1 HP regen/turn per rank";
        default:                 return "?";
    }
}

// ---------------------------------------------------------------------------
// Character
// ---------------------------------------------------------------------------

namespace {

int baseMaxHp(ClassId c, int level) {
    switch (c) {
        case ClassId::Warrior: return 40 + (level - 1) * 7;
        case ClassId::Mage:    return 26 + (level - 1) * 4;
        case ClassId::Rogue:   return 30 + (level - 1) * 5;
        case ClassId::Paladin: return 34 + (level - 1) * 6;
        case ClassId::Necromancer: return 30 + (level - 1) * 5;
    }
    return 30;
}

int baseResource(ClassId c, int level) {
    switch (c) {
        case ClassId::Warrior: return 14 + (level - 1) * 2;
        case ClassId::Mage:    return 20 + (level - 1) * 3;
        case ClassId::Rogue:   return 16 + (level - 1) * 3;
        case ClassId::Paladin: return 16 + (level - 1) * 3;
        case ClassId::Necromancer: return 16 + (level - 1) * 3;
    }
    return 16;
}

int baseAttack(ClassId c, int level) {
    switch (c) {
        case ClassId::Warrior: return 7 + (level - 1) * 2;
        case ClassId::Mage:    return 4 + (level - 1) * 1;
        case ClassId::Rogue:   return 6 + (level - 1) * 2;
        case ClassId::Paladin: return 6 + (level - 1) * 2;
        case ClassId::Necromancer: return 5 + (level - 1) * 1;
    }
    return 6;
}

int baseDefense(ClassId c, int level) {
    switch (c) {
        case ClassId::Warrior: return 3 + (level - 1);
        case ClassId::Mage:    return 1 + (level - 1) / 2;
        case ClassId::Rogue:   return 2 + (level - 1) / 2;
        case ClassId::Paladin: return 2 + (level - 1);
        case ClassId::Necromancer: return 2 + (level - 1) / 2;
    }
    return 2;
}

int baseCrit(ClassId c, int level) {
    switch (c) {
        case ClassId::Warrior: return 5;
        case ClassId::Mage:    return 5;
        case ClassId::Rogue:   return 10 + (level - 1) / 4;
        case ClassId::Paladin: return 5;
        case ClassId::Necromancer: return 5;
    }
    return 5;
}

int baseCritBonus(ClassId c) {
    switch (c) {
        case ClassId::Warrior: return 10;
        case ClassId::Mage:    return 15;
        case ClassId::Rogue:   return 20;
        case ClassId::Paladin: return 10;
        case ClassId::Necromancer: return 15;
    }
    return 10;
}

} // namespace

Character::Character(ClassId id)
    : classId_(id),
      hp_(baseMaxHp(id, 1)),
      resource_(baseResource(id, 1)) {}

EffectiveStats Character::stats(const Vault& vault) const {
    EffectiveStats s;
    s.maxHp        = baseMaxHp(classId_, level_);
    s.maxResource  = baseResource(classId_, level_);
    s.attack       = baseAttack(classId_, level_);
    s.defense      = baseDefense(classId_, level_);
    s.critChance   = baseCrit(classId_, level_);
    s.critBonus    = baseCritBonus(classId_);

    if (classId_ == ClassId::Paladin) s.healAmpPct += 25;
    if (classId_ == ClassId::Necromancer) s.lifeStealPct += 10;

    int dmgPct = 0, defPct = 0, hpPct = 0;

    for (const int e : vault.equipped) {
        if (!vault.hasItem(e)) continue;
        const Item& it = vault.items[static_cast<std::size_t>(e)];
        if (it.slot == Slot::Weapon) s.attack += it.power;
        else if (isDefenseSlot(it.slot)) s.defense += it.power;
        for (const auto& a : it.affixes) {
            switch (a.type) {
                case AffixType::DamagePct:   dmgPct += a.value; break;
                case AffixType::Defense:     defPct += a.value; break;
                case AffixType::MaxHp:       hpPct += a.value; break;
                case AffixType::CritChance:  s.critChance += a.value; break;
                case AffixType::CritBonus:   s.critBonus += a.value; break;
                case AffixType::LifeSteal:   s.lifeStealPct += a.value; break;
                case AffixType::Mana:        s.maxResource += a.value; break;
                case AffixType::ManaRegen:   s.manaRegenPerTurn += a.value; break;
                case AffixType::XpGain:      s.xpGainPct += a.value; break;
                case AffixType::Regen:       s.regenPerTurn += a.value; break;
            }
        }
        for (const auto& r : it.runes) {
            switch (r.type) {
                case RuneType::Power:    dmgPct += r.value; break;
                case RuneType::Vitality: hpPct += r.value; break;
                case RuneType::Warding:  defPct += r.value; break;
                case RuneType::Flow:     s.manaRegenPerTurn += r.value; break;
                case RuneType::Finesse:  s.critChance += r.value; break;
            }
        }
        switch (it.relic) {
            case RelicPower::Thorns:      s.thornsPct += 20; break;
            case RelicPower::Potent:      s.potionBonusPct += 25; break;
            case RelicPower::GoldenTouch: s.goldGainPct += 20; break;
            case RelicPower::Dodge:       s.dodgeChance += 10; break;
            case RelicPower::Aegis:       s.aegisGuard = std::max(s.aegisGuard, 50); break;
            case RelicPower::Scavenger:   s.scavengePerKill += 1; break;
            case RelicPower::None:
            case RelicPower::kNumRelics:  break;
        }
    }

    s.attack = s.attack + s.attack * dmgPct / 100;
    s.defense = s.defense + s.defense * defPct / 100;
    s.maxHp = s.maxHp + s.maxHp * hpPct / 100;

    // Passive training (permanent, capped at kTrainMaxRank per discipline)
    s.attack      += 3 * training_[0];
    s.maxHp       += 10 * training_[1];
    s.maxResource += 3 * training_[2];
    s.defense     += training_[3];
    s.regenPerTurn += training_[4];

    // Rift Aspect perks (permanent, chosen per Ascension)
    if (vault.perks[static_cast<std::size_t>(PerkId::Insight)])  s.xpGainPct += 10;
    if (vault.perks[static_cast<std::size_t>(PerkId::Vitals)])   s.maxHp += s.maxHp * 8 / 100;
    if (vault.perks[static_cast<std::size_t>(PerkId::Leeching)]) s.lifeStealPct += 2;
    if (vault.perks[static_cast<std::size_t>(PerkId::Bulwark)])  s.defense += 6;
    if (vault.perks[static_cast<std::size_t>(PerkId::Heirloom)]) s.goldGainPct += 20;
    if (vault.perks[static_cast<std::size_t>(PerkId::Greed)])        s.lootBonusPct += 50;
    if (vault.perks[static_cast<std::size_t>(PerkId::Regeneration)]) s.regenPerTurn += 2;
    if (vault.perks[static_cast<std::size_t>(PerkId::Evasion)])      s.dodgeChance += 8;
    if (vault.perks[static_cast<std::size_t>(PerkId::Bargain)])      s.vendorDiscountPct += 20;

    // Set bonuses (2-piece / 3-piece)
    const int titanic  = setPieces(vault, SetId::Titanic);
    const int infernal = setPieces(vault, SetId::Infernal);
    const int frost    = setPieces(vault, SetId::Frostbound);
    const int voidwalk = setPieces(vault, SetId::Voidwalk);
    if (titanic >= 3)       s.regenPerTurn += 3;
    if (titanic >= 2)       s.maxHp    += s.maxHp * 15 / 100;
    if (infernal >= 3)      s.critBonus += 20;
    if (infernal >= 2)      s.attack   += s.attack * 12 / 100;
    if (frost >= 3)         s.manaRegenPerTurn += 3;
    if (frost >= 2)         s.maxResource += s.maxResource * 10 / 100;
    if (voidwalk >= 3)      s.lifeStealPct += 3;
    if (voidwalk >= 2)      s.defense  += s.defense * 10 / 100;

    // compounding Rift Mastery — the endless-mode lever
    const int mastery = std::min(vault.mastery, kMasteryCap);
    const double aM = std::pow(1.08, static_cast<double>(mastery));
    const double hM = std::pow(1.06, static_cast<double>(mastery));
    const double rM = std::pow(1.05, static_cast<double>(mastery));
    s.attack = static_cast<int>(static_cast<double>(s.attack) * aM);
    s.maxHp = static_cast<int>(static_cast<double>(s.maxHp) * hM);
    s.maxResource = static_cast<int>(static_cast<double>(s.maxResource) * rM);
    return s;
}

const Spell& Character::spell(int branch, int depth) const {
    return classSpells(classId_)[static_cast<std::size_t>(branch * 4 + depth)];
}

static int nodeCost(int depth) { return depth + 1; }

bool Character::spendPoint(int branch, int depth) {
    const int idx = branch * 4 + depth;
    if (branch < 0 || branch > 2 || depth < 0 || depth > 3) return false;
    if (unlocked_.at(static_cast<std::size_t>(idx))) return false;
    if (depth > 0 && !unlocked_.at(static_cast<std::size_t>(idx - 1))) return false;
    if (nodeCost(depth) > skillPoints_) return false;
    skillPoints_ -= nodeCost(depth);
    unlocked_[static_cast<std::size_t>(idx)] = true;
    return true;
}

void Character::respec() {
    skillPoints_ += pointsSpent();
    unlocked_ = {};
}

int Character::pointsSpent() const {
    int total = 0;
    for (int i = 0; i < 12; ++i) {
        if (!unlocked_.at(static_cast<std::size_t>(i))) continue;
        total += nodeCost(i % 4);
    }
    return total;
}

int Character::trainRank(TrainId t) const {
    const int i = static_cast<int>(t);
    if (i < 0 || i >= kNumTrains) return 0;
    return training_[static_cast<std::size_t>(i)];
}

bool Character::train(TrainId t) {
    const int i = static_cast<int>(t);
    if (i < 0 || i >= kNumTrains) return false;
    int& rank = training_[static_cast<std::size_t>(i)];
    if (rank >= kTrainMaxRank || skillPoints_ < 1) return false;
    skillPoints_ -= 1;
    ++rank;
    return true;
}

int Character::trainingSpent() const {
    int total = 0;
    for (const int r : training_) total += r;
    return total;
}

void Character::gainXp(int amount, int gainPct) {
    if (amount <= 0) return;
    int gained = amount + amount * gainPct / 100;
    xp_ += gained;
    while (xp_ >= xpToNext() && level_ < 999) {
        xp_ -= xpToNext();
        const int prevMax = baseMaxHp(classId_, level_);
        const int prevRes = baseResource(classId_, level_);
        ++level_;
        ++skillPoints_;
        hp_ += std::max(0, baseMaxHp(classId_, level_) - prevMax);
        resource_ += std::max(0, baseResource(classId_, level_) - prevRes);
    }
    if (hp_ <= 0) hp_ = baseMaxHp(classId_, level_);   // never dead after a level
}

void Character::healHp(int n, int cap) {
    const int m = cap > 0 ? cap : baseMaxHp(classId_, level_);
    hp_ = std::min(m, hp_ + std::max(0, n));
}

void Character::takeDamage(int n) { hp_ = std::max(0, hp_ - std::max(0, n)); }
void Character::spendResource(int n) { resource_ = std::max(0, resource_ - std::max(0, n)); }
void Character::restoreResource(int n, int cap) {
    const int m = cap > 0 ? cap : baseResource(classId_, level_);
    resource_ = std::min(resource_ + std::max(0, n), m);
}
void Character::restoreAll(int hpCap, int resCap) {
    hp_ = hpCap > 0 ? hpCap : baseMaxHp(classId_, level_);
    resource_ = resCap > 0 ? resCap : baseResource(classId_, level_);
}

Character::Snapshot Character::snapshot() const {
    return Snapshot{ classId_, level_, xp_, skillPoints_, hp_, resource_, floor_, hardcore_, unlocked_, training_ };
}

void Character::restore(const Snapshot& s) {
    classId_ = s.id;
    level_ = s.level;
    xp_ = s.xp;
    skillPoints_ = s.skillPoints;
    hp_ = s.hp;
    resource_ = s.resource;
    floor_ = s.floor;
    hardcore_ = s.hardcore;
    unlocked_ = s.unlocked;
    training_ = s.training;
    if (hp_ <= 0) hp_ = baseMaxHp(classId_, level_);
}

} // namespace rpg