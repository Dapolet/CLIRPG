#include "character.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace rpg {

namespace {

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
    }
    return s;
}

} // namespace

const std::vector<Spell>& classSpells(ClassId c) {
    static const std::vector<Spell> warrior = buildSpells(ClassId::Warrior);
    static const std::vector<Spell> mage    = buildSpells(ClassId::Mage);
    static const std::vector<Spell> rogue   = buildSpells(ClassId::Rogue);
    switch (c) {
        case ClassId::Warrior: return warrior;
        case ClassId::Mage:    return mage;
        case ClassId::Rogue:   return rogue;
    }
    return warrior;
}

const char* className(ClassId c) {
    switch (c) {
        case ClassId::Warrior: return "Warrior";
        case ClassId::Mage:    return "Mage";
        case ClassId::Rogue:   return "Rogue";
    }
    return "?";
}

const char* resourceName(ClassId c) {
    switch (c) {
        case ClassId::Warrior: return "Stamina";
        case ClassId::Mage:    return "Mana";
        case ClassId::Rogue:   return "Energy";
    }
    return "?";
}

int spellDamage(const Spell& s, int level) {
    return std::max(1, s.potency + static_cast<int>(s.lvlScale * static_cast<double>(level)));
}

int spellHeal(const Spell& s, int level) {
    return std::max(1, s.healPower + static_cast<int>(s.lvlScale * static_cast<double>(level)));
}

int resourceRegenPerTurn(ClassId c) {
    return c == ClassId::Mage ? 2 : 1;
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
    }
    return 30;
}

int baseResource(ClassId c, int level) {
    switch (c) {
        case ClassId::Warrior: return 14 + (level - 1) * 2;
        case ClassId::Mage:    return 20 + (level - 1) * 3;
        case ClassId::Rogue:   return 16 + (level - 1) * 3;
    }
    return 16;
}

int baseAttack(ClassId c, int level) {
    switch (c) {
        case ClassId::Warrior: return 7 + (level - 1) * 2;
        case ClassId::Mage:    return 4 + (level - 1) * 1;
        case ClassId::Rogue:   return 6 + (level - 1) * 2;
    }
    return 6;
}

int baseDefense(ClassId c, int level) {
    switch (c) {
        case ClassId::Warrior: return 3 + (level - 1);
        case ClassId::Mage:    return 1 + (level - 1) / 2;
        case ClassId::Rogue:   return 2 + (level - 1) / 2;
    }
    return 2;
}

int baseCrit(ClassId c, int level) {
    switch (c) {
        case ClassId::Warrior: return 5;
        case ClassId::Mage:    return 5;
        case ClassId::Rogue:   return 10 + (level - 1) / 4;
    }
    return 5;
}

int baseCritBonus(ClassId c) {
    switch (c) {
        case ClassId::Warrior: return 10;
        case ClassId::Mage:    return 15;
        case ClassId::Rogue:   return 20;
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

    int dmgPct = 0, defPct = 0, hpPct = 0, resPct = 0;

    for (const int e : vault.equipped) {
        if (!vault.hasItem(e)) continue;
        const Item& it = vault.items[static_cast<std::size_t>(e)];
        if (it.slot == Slot::Weapon) s.attack += it.power;
        else if (it.slot == Slot::Armor) s.defense += it.power;
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
    }

    s.attack = s.attack + s.attack * dmgPct / 100;
    s.defense = s.defense + s.defense * defPct / 100;
    s.maxHp = s.maxHp + s.maxHp * hpPct / 100;
    s.maxResource = s.maxResource + s.maxResource * resPct / 100;

    // Rift Aspect perks (permanent, chosen per Ascension)
    if (vault.perks[static_cast<std::size_t>(PerkId::Insight)])  s.xpGainPct += 10;
    if (vault.perks[static_cast<std::size_t>(PerkId::Vitals)])   s.maxHp += s.maxHp * 8 / 100;
    if (vault.perks[static_cast<std::size_t>(PerkId::Leeching)]) s.lifeStealPct += 2;
    if (vault.perks[static_cast<std::size_t>(PerkId::Bulwark)])  s.defense += 6;

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
    const double aM = std::pow(1.08, static_cast<double>(vault.mastery));
    const double hM = std::pow(1.06, static_cast<double>(vault.mastery));
    const double rM = std::pow(1.05, static_cast<double>(vault.mastery));
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

void Character::gainXp(int amount, int gainPct) {
    if (amount <= 0) return;
    int gained = amount + amount * gainPct / 100;
    xp_ += gained;
    while (xp_ >= xpToNext() && level_ < 999) {
        xp_ -= xpToNext();
        ++level_;
        ++skillPoints_;
        hp_ = baseMaxHp(classId_, level_);
        resource_ = baseResource(classId_, level_);
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
    return Snapshot{ classId_, level_, xp_, skillPoints_, hp_, resource_, floor_, unlocked_ };
}

void Character::restore(const Snapshot& s) {
    classId_ = s.id;
    level_ = s.level;
    xp_ = s.xp;
    skillPoints_ = s.skillPoints;
    hp_ = s.hp;
    resource_ = s.resource;
    floor_ = s.floor;
    unlocked_ = s.unlocked;
    if (hp_ <= 0) hp_ = baseMaxHp(classId_, level_);
}

} // namespace rpg