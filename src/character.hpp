#pragma once

#include "core.hpp"
#include "items.hpp"

#include <array>
#include <string>
#include <vector>

namespace rpg {

enum class ClassId { Warrior, Mage, Rogue, Paladin };

const char* className(ClassId c);
const char* resourceName(ClassId c);

enum class SpellType { Attack, Heal, BuffSelf };

struct Spell {
    std::string name;
    SpellType type = SpellType::Attack;
    int cost = 0;
    int cooldown = 0;
    int potency = 0;                  // base damage
    double lvlScale = 0.0;            // + damage per level
    int hits = 1;
    int healPower = 0;                // + heal per level
    int buffAttack = 0;               // % self buff
    int buffDefense = 0;              // % self buff
    int buffTurns = 0;
    core::StatusEffect effect = core::StatusEffect::None;
    int effectTurns = 0;
    int armorShred = 0;               // enemy defense shred (debuff)
    int stunChancePct = 0;
    int critBonusSelf = 0;            // extra crit bonus for this cast
    core::Element element = core::Element::None;
    int enemyAtkDownPct = 0;          // enemy: Enfeeble power  (-% attack)
    int enemyVulnPct = 0;             // enemy: Vulnerable power (+% damage taken)
};

// 12 spells per class; index = branch * 4 + depth. Built once, cached.
const std::vector<Spell>& classSpells(ClassId c);
int spellDamage(const Spell& s, int level, int floor);
int spellHeal(const Spell& s, int level, int floor);
int resourceRegenPerTurn(ClassId c);
std::string spellBlurb(const Spell& s, int level, int floor, int attack);   // one-line effect summary

// Passive training: a permanent skill-point sink available at camp.
enum class TrainId { Might, Vitality, Focus, Tenacity, Fleetness, kNumTrains };
constexpr int kNumTrains = static_cast<int>(TrainId::kNumTrains);
constexpr int kTrainMaxRank = 5;
const char* trainName(TrainId t);
const char* trainDesc(TrainId t);

struct EffectiveStats {
    int maxHp = 0;
    int maxResource = 0;
    int attack = 0;
    int defense = 0;
    int critChance = 0;
    int critBonus = 0;
    int regenPerTurn = 0;
    int manaRegenPerTurn = 0;
    int lifeStealPct = 0;
    int xpGainPct = 0;
    int goldGainPct = 0;
    int lootBonusPct = 0;
    int dodgeChance = 0;
    int vendorDiscountPct = 0;
    int potionBonusPct = 0;
    int thornsPct = 0;
    int scavengePerKill = 0;
    int aegisGuard = 0;
    int healAmpPct = 0;
};

class Character {
public:
    explicit Character(ClassId id);

    ClassId classId() const { return classId_; }
    int level() const { return level_; }
    int xp() const { return xp_; }
    int xpToNext() const { return core::xpNeeded(level_); }
    int hp() const { return hp_; }
    int resource() const { return resource_; }
    int skillPoints() const { return skillPoints_; }
    int currentFloor() const { return floor_; }
    void setFloor(int f) { floor_ = f; }
    bool alive() const { return hp_ > 0; }
    bool hardcore() const { return hardcore_; }
    void setHardcore(bool hc) { hardcore_ = hc; }

    EffectiveStats stats(const Vault& vault) const;

    bool isUnlocked(int branch, int depth) const {
        const int i = branch * 4 + depth;
        if (i < 0 || i >= static_cast<int>(unlocked_.size())) return false;
        return unlocked_[static_cast<std::size_t>(i)];
    }
    const Spell& spell(int branch, int depth) const;
    bool spendPoint(int branch, int depth);
    void respec();
    int pointsSpent() const;
    std::array<bool, 12> tree() const { return unlocked_; }

    int  trainRank(TrainId t) const;
    bool train(TrainId t);                 // spend 1 skill point, cap kTrainMaxRank
    int  trainingSpent() const;

    void gainXp(int amount, int gainPct);
    void healHp(int n, int cap = 0);
    void takeDamage(int n);
    void spendResource(int n);
    void restoreResource(int n, int cap = 0);
    void restoreAll(int hpCap = 0, int resCap = 0);
    bool canCast(const Spell& s) const { return resource_ >= s.cost; }

    // serialization support
    struct Snapshot {
        ClassId id = ClassId::Warrior;
        int level = 1;
        int xp = 0;
        int skillPoints = 0;
        int hp = 0;
        int resource = 0;
        int floor = 1;
        bool hardcore = false;
        std::array<bool, 12> unlocked{};
        std::array<int, kNumTrains> training{};
    };
    Snapshot snapshot() const;
    void restore(const Snapshot& s);

private:
    ClassId classId_ = ClassId::Warrior;
    int level_ = 1;
    int xp_ = 0;
    int skillPoints_ = 2;          // enough to pick an opening spell
    int hp_ = 0;
    int resource_ = 0;
    int floor_ = 1;
    bool hardcore_ = false;
    std::array<bool, 12> unlocked_{};
    std::array<int, kNumTrains> training_{};
};

} // namespace rpg