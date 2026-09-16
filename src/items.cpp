#include "items.hpp"

#include "ui.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace rpg {

namespace {

// ---------------------------------------------------------------------------
// display helpers
// ---------------------------------------------------------------------------

constexpr const char* kRarityNames[kNumRarities] = {
    "Common", "Uncommon", "Rare", "Epic", "Legendary",
};
constexpr const char* kSlotWords[kNumSlots] = {
    "Weapon", "Armor", "Ring", "Amulet", "Helm", "Gloves", "Boots",
};
constexpr const char* kWeaponBases[10] = {
    "Sword", "Axe", "Mace", "Dagger", "Warhammer",
    "Spear", "Bow", "Scythe", "Longsword", "Staff",
};
constexpr const char* kArmorBases[10] = {
    "Hauberk", "Plate", "Robe", "Leathers", "Vestments",
    "Brigandine", "Scale", "Gambeson", "Cuirass", "Greatcloak",
};
constexpr const char* kHelmBases[6] = {
    "Helm", "Crown", "Skullcap", "Sallet", "Visor", "Circlet",
};
constexpr const char* kGlovesBases[6] = {
    "Gauntlets", "Gloves", "Bracers", "Handwraps", "Splints", "Claws",
};
constexpr const char* kBootsBases[6] = {
    "Sabatons", "Greaves", "Boots", "Treads", "Waders", "Sollerets",
};
constexpr const char* kTrinketBaseNames[2] = { "Ring", "Amulet" };
constexpr const char* kTierNames[kNumTiers] = {
    "Iron", "Steel", "Mythril", "Adamant", "Void",
};
constexpr const char* kAffixNames[kNumAffixTypes] = {
    "Damage", "Crit Chance", "Crit Damage", "Life Steal",
    "Max HP", "Defense", "HP Regen",
    "Mana", "Mana Regen", "XP Gain",
};
constexpr const char* kRuneNames[5] = {
    "Power", "Vitality", "Warding", "Flow", "Finesse",
};

const std::vector<std::string> kWordsDamage = { "Venomous", "Fierce", "Savage", "Bloodsoaked", "Infernal" };
const std::vector<std::string> kWordsCritC  = { "Shifty", "Keen", "Precise", "Cunning" };
const std::vector<std::string> kWordsCritB  = { "Deadly", "Merciless", "Warlike", "Ruinous" };
const std::vector<std::string> kWordsLeech  = { "Vampiric", "Leeching", "Draining", "Sanguine" };
const std::vector<std::string> kWordsHp     = { "Sturdy", "Hulking", "Titanic", "Colossal" };
const std::vector<std::string> kWordsDef    = { "Reinforced", "Bulwark", "Impenetrable", "Stalwart" };
const std::vector<std::string> kWordsRegen  = { "Healing", "Regenerative", "Blessed" };
const std::vector<std::string> kWordsMana   = { "Arcane", "Mystical", "Ethereal", "Empowered" };
const std::vector<std::string> kWordsManReg = { "Flowing", "Serene", "Soothing" };
const std::vector<std::string> kWordsXp     = { "Insightful", "Scholarly", "Enlightened" };

} // namespace

int slotIndex(Slot s) { return static_cast<int>(s); }
const char* rarityName(Rarity r) { return kRarityNames[static_cast<int>(r)]; }
const char* slotName(Slot s)     { return kSlotWords[static_cast<int>(s)]; }
bool isDefenseSlot(Slot s) {
    switch (s) {
        case Slot::Armor: case Slot::Helm: case Slot::Gloves: case Slot::Boots:
            return true;
        default:
            return false;
    }
}
const char* tierName(ItemTier t) { return kTierNames[static_cast<int>(t)]; }
const char* affixName(AffixType t) { return kAffixNames[static_cast<int>(t)]; }

Rarity rarityByIndex(int i) { return static_cast<Rarity>(std::clamp(i, 0, kNumRarities - 1)); }
ItemTier tierByIndex(int i) { return static_cast<ItemTier>(std::clamp(i, 0, kNumTiers - 1)); }
const char* runeName(RuneType t) {
    return kRuneNames[std::clamp(static_cast<int>(t), 0, 4)];
}

const char* setName(SetId s) {
    static const char* kSetNames[] = { "None", "Titanic", "Infernal", "Frostbound", "Voidwalk", "?" };
    return kSetNames[std::clamp(static_cast<int>(s), 0, static_cast<int>(SetId::kNumSets))];
}

const char* perkName(PerkId p) {
    static const char* kPerkNames[] = {
        "Heirloom", "Runeforge", "Insight", "Vitals", "Leeching", "Bulwark", "?"
    };
    return kPerkNames[std::clamp(static_cast<int>(p), 0, static_cast<int>(PerkId::kNumPerks))];
}

int socketsFor(Rarity r) {
    switch (r) {
        case Rarity::Common:    return 0;
        case Rarity::Uncommon:  return 1;
        case Rarity::Rare:      return 1;
        case Rarity::Epic:      return 2;
        case Rarity::Legendary: return 3;
    }
    return 0;
}

int tierUnlockFloor(ItemTier t) {
    switch (t) {
        case ItemTier::Iron:    return 1;
        case ItemTier::Steel:   return 10;
        case ItemTier::Mythril: return 24;
        case ItemTier::Adamant: return 45;
        case ItemTier::Void:    return 75;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// affix pools
// ---------------------------------------------------------------------------

namespace {

struct PoolEntry {
    AffixType type;
    int lo;             // base roll range
    int hi;
    double gain;        // + per iLvl
    const std::vector<std::string>* words;
};

const std::vector<PoolEntry>& poolFor(Slot s) {
    static const std::vector<PoolEntry> weapon = {
        { AffixType::DamagePct,  4,  8, 0.35, &kWordsDamage },
        { AffixType::CritChance, 1,  3, 0.05, &kWordsCritC  },
        { AffixType::CritBonus,  6, 16, 0.80, &kWordsCritB  },
        { AffixType::LifeSteal,  1,  3, 0.05, &kWordsLeech  },
        { AffixType::XpGain,     2,  6, 0.10, &kWordsXp     },
    };
    static const std::vector<PoolEntry> armor = {
        { AffixType::MaxHp,      3,  7, 0.30, &kWordsHp     },
        { AffixType::Defense,    2,  6, 0.25, &kWordsDef    },
        { AffixType::Regen,      1,  3, 0.05, &kWordsRegen  },
        { AffixType::LifeSteal,  1,  2, 0.03, &kWordsLeech  },
        { AffixType::XpGain,     2,  5, 0.10, &kWordsXp     },
    };
    static const std::vector<PoolEntry> ring = {
        { AffixType::Mana,       5, 14, 0.80, &kWordsMana   },
        { AffixType::ManaRegen,  1,  3, 0.05, &kWordsManReg },
        { AffixType::CritChance, 1,  4, 0.05, &kWordsCritC  },
        { AffixType::LifeSteal,  1,  3, 0.05, &kWordsLeech  },
        { AffixType::XpGain,     2,  8, 0.10, &kWordsXp     },
    };
    static const std::vector<PoolEntry> amulet = {
        { AffixType::Mana,       8, 18, 1.00, &kWordsMana   },
        { AffixType::ManaRegen,  1,  3, 0.05, &kWordsManReg },
        { AffixType::CritChance, 1,  4, 0.05, &kWordsCritC  },
        { AffixType::CritBonus,  8, 20, 1.00, &kWordsCritB  },
        { AffixType::MaxHp,      3,  7, 0.25, &kWordsHp     },
    };
    static const std::vector<PoolEntry> helm = {
        { AffixType::MaxHp,    3,  6, 0.30, &kWordsHp     },
        { AffixType::Defense,  2,  5, 0.25, &kWordsDef    },
        { AffixType::Regen,    1,  3, 0.05, &kWordsRegen  },
        { AffixType::Mana,     4, 12, 0.60, &kWordsMana   },
        { AffixType::XpGain,   2,  5, 0.10, &kWordsXp     },
    };
    static const std::vector<PoolEntry> gloves = {
        { AffixType::CritChance, 1,  4, 0.05, &kWordsCritC  },
        { AffixType::CritBonus,  6, 16, 0.80, &kWordsCritB  },
        { AffixType::LifeSteal,  1,  3, 0.05, &kWordsLeech  },
        { AffixType::Defense,    2,  5, 0.25, &kWordsDef    },
    };
    static const std::vector<PoolEntry> boots = {
        { AffixType::Defense,    2,  6, 0.25, &kWordsDef    },
        { AffixType::Regen,      1,  3, 0.05, &kWordsRegen  },
        { AffixType::ManaRegen,  1,  3, 0.05, &kWordsManReg },
        { AffixType::MaxHp,      3,  6, 0.30, &kWordsHp     },
        { AffixType::XpGain,     2,  5, 0.10, &kWordsXp     },
    };
    switch (s) {
        case Slot::Weapon: return weapon;
        case Slot::Armor:  return armor;
        case Slot::Ring:   return ring;
        case Slot::Amulet: return amulet;
        case Slot::Helm:   return helm;
        case Slot::Gloves: return gloves;
        case Slot::Boots:  return boots;
    }
    return weapon;
}

int baseTierPower(ItemTier t, bool weapon) {
    static const int kWeapon[5] = { 5, 11, 20, 34, 55 };
    static const int kArmor[5]  = { 3,  7, 12, 20, 32 };
    return weapon ? kWeapon[static_cast<int>(t)] : kArmor[static_cast<int>(t)];
}

int affixCount(Rarity r) {
    switch (r) {
        case Rarity::Rare:      return 2;
        case Rarity::Epic:      return 3;
        case Rarity::Legendary: return 4;
        default:                return 1;
    }
}

} // namespace

std::string Rune::describe() const {
    return std::string(runeName(type)) + " +" + std::to_string(value);
}

std::string Item::describe(int floor) const {
    if (consumable) {
        const int f = std::max(floor, 1);
        std::string out = name;
        if (count > 1) out += " x" + std::to_string(count);
        if (potionKind == PotionKind::Healing)
            out += " heals " + std::to_string(potionAmount(PotionKind::Healing, f)) + " HP";
        else if (potionKind == PotionKind::Mana)
            out += " restores " + std::to_string(potionAmount(PotionKind::Mana, f)) + " resource";
        return out;
    }
    std::string out = name + " [" + rarityName(rarity) + " " + tierName(tier) +
                      " iLvl " + std::to_string(iLvl) + "]";
    if (setTag != SetId::None) out += " [" + std::string(setName(setTag)) + " set]";
    if (power > 0) out += " power " + std::to_string(power);
    if (!affixes.empty()) {
        out += " (";
        for (const auto& a : affixes)
            out += a.prefix + " " + affixName(a.type) + " +" + std::to_string(a.value) + ", ";
        out.pop_back();
        out.pop_back();
        out += ")";
    }
    if (!runes.empty() || freeSockets() > 0) {
        out += " [";
        for (const auto& r : runes) out += std::string(ui::gly(ui::G_SOCKET)) + r.describe() + ", ";
        for (int i = static_cast<int>(runes.size()); i < socketsFor(rarity); ++i)
            out += ui::gly(ui::G_EMPTY);
        out += "]";
    }
    return out;
}

// ---------------------------------------------------------------------------
// Vault
// ---------------------------------------------------------------------------

int Vault::equippedIndex(Slot s) const {
    const int i = equipped[slotIndex(s)];
    return hasItem(i) ? i : -1;
}

void Vault::equip(int itemIndex) {
    if (!hasItem(itemIndex)) return;
    const Item& it = items[static_cast<std::size_t>(itemIndex)];
    if (it.isPot()) return;
    equipped[slotIndex(it.slot)] = itemIndex;
}

void Vault::unequip(Slot s) { equipped[slotIndex(s)] = -1; }

void Vault::add(const Item& it) {
    Item copy = it;
    if (copy.uid <= 0) copy.uid = nextUid++;
    if (copy.consumable && copy.count <= 0) copy.count = 1;
    items.push_back(copy);
}

void Vault::addPotion(const Item& it) {
    if (it.potionKind == PotionKind::None) return;
    for (auto& existing : items) {
        if (existing.potionKind == it.potionKind) {
            existing.count += std::max(it.count, 1);
            return;
        }
    }
    Item copy = it;
    copy.consumable = true;
    copy.count = std::max(it.count, 1);
    copy.uid = nextUid++;
    items.push_back(copy);
}

bool Vault::usePotion(int idx) {
    if (!hasItem(idx)) return false;
    Item& it = items[static_cast<std::size_t>(idx)];
    if (!it.isPot()) return false;
    if (--it.count > 0) return true;
    const PotionKind k = it.potionKind;
    removeAt(idx);
    bool anyLeft = false;
    for (const auto& x : items)
        if (x.potionKind == k) { anyLeft = true; break; }
    if (!anyLeft) {
        for (auto& b : belt)
            if (b == static_cast<int>(k) + 1) b = 0;
    }
    return true;
}

int Vault::indexOfUid(int uid) const {
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].uid == uid) return static_cast<int>(i);
    return -1;
}

PotionKind Vault::beltKind(int n) const {
    if (n < 0 || n > 1) return PotionKind::None;
    const int code = belt[static_cast<std::size_t>(n)];
    if (code <= 0) return PotionKind::None;
    return static_cast<PotionKind>(code - 1);
}

void Vault::bindBelt(int n, PotionKind k) {
    if (n < 0 || n > 1) return;
    belt[static_cast<std::size_t>(n)] = (k == PotionKind::None) ? 0 : static_cast<int>(k) + 1;
}

int Vault::beltIndex(int n) const {
    const PotionKind k = beltKind(n);
    if (k == PotionKind::None) return -1;
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].potionKind == k) return static_cast<int>(i);
    return -1;
}

bool Vault::removeAt(int idx) {
    if (!hasItem(idx)) return false;
    for (auto& e : equipped)
        if (e == idx) e = -1;
    items.erase(items.begin() + idx);
    for (auto& e : equipped)
        if (e > idx) --e;
    return true;
}

bool Vault::hasItem(int idx) const {
    return idx >= 0 && static_cast<std::size_t>(idx) < items.size();
}

void Vault::clear() {
    gold = shards = essence = 0;
    items.clear();
    equipped = { -1, -1, -1, -1, -1, -1, -1 };
    belt = { 0, 0 };
    bestFloor = 1;
    bossesSlain = kills = deaths = legendaryFound = 0;
    mastery = aspect = 0;
    totalGoldEarned = 0;
    nextUid = 1;
    runes.clear();
    perks = {};
    bestiary = Bestiary{};
}

int setPieces(const Vault& vault, SetId s) {
    if (s == SetId::None) return 0;
    int n = 0;
    for (const int e : vault.equipped) {
        if (!vault.hasItem(e)) continue;
        if (vault.items[static_cast<std::size_t>(e)].setTag == s) ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// loot generation
// ---------------------------------------------------------------------------

namespace {

Rarity rollRarity(int floor, core::Rng& rng) {
    const int w[] = {
        std::max(100 - 3 * floor, 5),
        55,
        std::min(20 + floor, 60),
        std::min(4 + floor / 2, 25),
        std::min(floor / 3, 8),
    };
    long long total = 0;
    for (int i = 0; i < kNumRarities; ++i) total += w[i];
    long long r = static_cast<long long>(rng.roll01() * static_cast<double>(total));
    for (int i = 0; i < kNumRarities; ++i) {
        if (r < w[i]) return rarityByIndex(i);
        r -= w[i];
    }
    return Rarity::Common;
}

ItemTier pickTier(int floor, core::Rng& rng) {
    std::vector<ItemTier> unlocked;
    for (int i = 0; i < kNumTiers; ++i) {
        const auto t = tierByIndex(i);
        if (tierUnlockFloor(t) <= floor) unlocked.push_back(t);
    }
    if (unlocked.empty()) return ItemTier::Iron;
    // bias toward the newest tier so progression is felt
    const int top = static_cast<int>(unlocked.size()) - 1;
    const int pick = rng.chance(0.55) ? top : static_cast<int>(rng.pick(unlocked.size()));
    return unlocked[static_cast<std::size_t>(pick)];
}

int rollAffixValue(const PoolEntry& e, int iLvl, core::Rng& rng) {
    const int base = rng.roll(e.lo, e.hi);
    const int scaled = static_cast<int>(e.gain * static_cast<double>(iLvl - 1));
    return base + scaled;
}

std::vector<int> distinctTypes(const std::vector<PoolEntry>& pool, int n, core::Rng& rng) {
    std::vector<int> idx;
    for (std::size_t i = 0; i < pool.size(); ++i) idx.push_back(static_cast<int>(i));
    for (std::size_t i = idx.size(); i > 1; --i)
        std::swap(idx[i - 1], idx[rng.pick(i)]);
    idx.resize(static_cast<std::size_t>(n));
    return idx;
}

void rerollAffixes(Item& it, core::Rng& rng) {
    const auto& pool = poolFor(it.slot);
    const int n = affixCount(it.rarity);
    const auto picks = distinctTypes(pool, std::min(n, static_cast<int>(pool.size())), rng);
    it.affixes.clear();
    for (const int p : picks) {
        const PoolEntry& e = pool[static_cast<std::size_t>(p)];
        Affix a;
        a.type = e.type;
        a.value = rollAffixValue(e, it.iLvl, rng);
        if (!e.words->empty()) a.prefix = (*e.words)[rng.pick(e.words->size())];
        it.affixes.push_back(a);
    }
}

const char* gearBase(const Item& it) {
    if (it.slot == Slot::Weapon) {
        const int seed = static_cast<int>(it.tier) * 13 + static_cast<int>(it.rarity) * 7
                       + it.iLvl * 5 + it.power;
        return kWeaponBases[seed % 10];
    }
    if (it.slot == Slot::Armor) {
        const int seed = static_cast<int>(it.tier) * 13 + static_cast<int>(it.rarity) * 7
                       + it.iLvl * 5 + it.power;
        return kArmorBases[seed % 10];
    }
    const int seed = static_cast<int>(it.tier) * 13 + static_cast<int>(it.rarity) * 7
                   + it.iLvl * 5 + it.power;
    switch (it.slot) {
        case Slot::Helm:   return kHelmBases[seed % 6];
        case Slot::Gloves: return kGlovesBases[seed % 6];
        case Slot::Boots:  return kBootsBases[seed % 6];
        default:           return kTrinketBaseNames[slotIndex(it.slot) - 2];
    }
}

void refreshName(Item& it) {
    std::string subtype = gearBase(it);
    if (it.slot == Slot::Weapon || it.slot == Slot::Armor) {
        const std::size_t sp = it.name.rfind(' ');
        if (sp != std::string::npos && sp + 1 < it.name.size())
            subtype = it.name.substr(sp + 1);
    }
    const std::string base = std::string(tierName(it.tier)) + " " + subtype;
    it.name = it.affixes.empty() ? base : it.affixes.front().prefix + " " + base;
}

} // namespace

int potionAmount(PotionKind k, int floor) {
    const int f = std::max(floor, 1);
    switch (k) {
        case PotionKind::Healing: return 55 + 22 * f;
        case PotionKind::Mana:    return 45 + 14 * f;
        case PotionKind::None:    return 0;
    }
    return 0;
}

int potionHeal(const Item& it, int floor) {
    return it.potionKind == PotionKind::Healing ? potionAmount(PotionKind::Healing, floor) : 0;
}

int potionMana(const Item& it, int floor) {
    return it.potionKind == PotionKind::Mana ? potionAmount(PotionKind::Mana, floor) : 0;
}

Item makePotion(int level, core::Rng& rng) {
    Item it;
    it.consumable = true;
    it.iLvl = std::max(1, level);
    it.count = 1;
    it.rarity = rng.chance(0.85) ? Rarity::Common : Rarity::Uncommon;
    if (rng.chance(0.5)) {
        it.potionKind = PotionKind::Healing;
        it.name = "Healing Draught";
        it.heal = potionAmount(PotionKind::Healing, 1);
    } else {
        it.potionKind = PotionKind::Mana;
        it.name = "Mana Draught";
        it.manaRestore = potionAmount(PotionKind::Mana, 1);
    }
    return it;
}

Item makeVendorPotion(int level, bool mana) {
    Item it;
    it.consumable = true;
    it.iLvl = std::max(1, level);
    it.count = 1;
    it.rarity = Rarity::Common;
    if (mana) {
        it.potionKind = PotionKind::Mana;
        it.name = "Mana Draught";
        it.manaRestore = potionAmount(PotionKind::Mana, 1);
    } else {
        it.potionKind = PotionKind::Healing;
        it.name = "Healing Draught";
        it.heal = potionAmount(PotionKind::Healing, 1);
    }
    return it;
}

Item makeGear(int floor, core::Rng& rng, Rarity minRarity) {
    Item it;
    it.consumable = false;
    it.rarity = rollRarity(floor, rng);
    if (it.rarity < minRarity) it.rarity = minRarity;
    it.tier   = pickTier(floor, rng);
    // drops LAG the floor so blacksmith upgrades (cap = floor) matter
    const int gap = std::min(floor / 5, 6);
    it.iLvl   = std::max(1, floor - rng.roll(0, gap));

    const int s = rng.roll(0, kNumSlots - 1);
    it.slot = static_cast<Slot>(s);
    if (it.slot == Slot::Weapon)
        it.power = baseTierPower(it.tier, true) + static_cast<int>(1.2 * static_cast<double>(it.iLvl - 1));
    else if (isDefenseSlot(it.slot))
        it.power = baseTierPower(it.tier, false) + static_cast<int>(0.8 * static_cast<double>(it.iLvl - 1));
    else
        it.power = 0;

    rerollAffixes(it, rng);
    refreshName(it);
    return it;
}

Rune makeRune(int floor, core::Rng& rng) {
    Rune r;
    const int f = std::max(1, floor);
    r.type = static_cast<RuneType>(rng.pick(5));
    switch (r.type) {
        case RuneType::Power:    r.value = rng.roll(6, 10) + f / 4;   break;
        case RuneType::Vitality: r.value = rng.roll(6, 10) + f / 5;   break;
        case RuneType::Warding:  r.value = rng.roll(5, 8)  + f / 6;   break;
        case RuneType::Flow:     r.value = rng.roll(1, 3)  + f / 20;  break;
        case RuneType::Finesse:  r.value = rng.roll(2, 5)  + f / 25;  break;
    }
    return r;
}

std::vector<Rune> rollRunestoneLoot(int floor, bool boss, core::Rng& rng, bool boosted) {
    std::vector<Rune> out;
    if (boss) {
        out.push_back(makeRune(floor, rng));
        if (boosted) out.push_back(makeRune(floor, rng));
        return out;
    }
    const double rate = std::min(0.03 + 0.004 * static_cast<double>(floor), 0.25);
    if (rng.chance(rate)) out.push_back(makeRune(floor, rng));
    if (boosted && rng.chance(rate)) out.push_back(makeRune(floor, rng));
    return out;
}

std::vector<Item> rollLoot(int floor, bool boss, core::Rng& rng) {
    std::vector<Item> out;
    if (boss) {
        // promise (plan A2): at least one guaranteed epic+ piece
        out.push_back(makeGear(std::max(floor - 1, 1), rng));
        out.push_back(makeGear(floor, rng, Rarity::Epic));
        return out;
    }
    const int potions = rng.roll(0, 1);
    for (int i = 0; i < potions; ++i) out.push_back(makePotion(floor, rng));
    const double rate = std::min(0.20 + 0.02 * static_cast<double>(floor), 0.85);
    if (rng.chance(rate)) out.push_back(makeGear(floor, rng));
    return out;
}

// ---------------------------------------------------------------------------
// blacksmith
// ---------------------------------------------------------------------------

bool canUpgrade(const Item& it, int floor) {
    return !it.isPot() && it.iLvl < floor;
}

void upgrade(Item& it) {
    if (it.isPot()) return;
    ++it.iLvl;
    if (it.slot == Slot::Weapon || isDefenseSlot(it.slot)) it.power += 1;
}

void reforge(Item& it, core::Rng& rng) {
    if (it.isPot()) return;
    rerollAffixes(it, rng);
    refreshName(it);
}

bool canAwaken(const Item& it) {
    if (it.isPot()) return false;
    const int c = static_cast<int>(it.affixes.size());
    return (it.rarity == Rarity::Rare && c < 2) ||
           (it.rarity == Rarity::Epic && c < 3) ||
           (it.rarity == Rarity::Legendary && c < 4);
}

void awaken(Item& it, core::Rng& rng) {
    if (!canAwaken(it)) return;
    rerollAffixes(it, rng);
    refreshName(it);
}

bool socketRune(Item& it, const Rune& rune) {
    if (it.isPot() || !it.hasFreeSocket()) return false;
    it.runes.push_back(rune);
    return true;
}

Rune extractRune(Item& it) {
    if (it.runes.empty()) return Rune{};
    const Rune r = it.runes.back();
    it.runes.pop_back();
    return r;
}

int sellPrice(const Item& it) {
    if (it.isPot()) return 0;
    return 8 * it.iLvl * (static_cast<int>(it.rarity) + 1) / 2;
}

int salvageShards(const Item& it) {
    if (it.isPot()) return 0;
    return 1 + it.iLvl / 20 + 2 * static_cast<int>(it.rarity);
}

int salvageEssence(const Item& it) {
    if (it.isPot()) return 0;
    return it.rarity >= Rarity::Epic ? 1 : 0;
}

int reforgeCost(const Item& it) {
    return 25 * (static_cast<int>(it.rarity) + 1) * (1 + it.iLvl / 8);
}

int upgradeCost(const Item& it) {
    return 5 * (static_cast<int>(it.tier) + 1) + it.iLvl / 2;
}

int awakenCost(const Item& it) {
    return 50 * (static_cast<int>(it.rarity) + 1) * (1 + it.iLvl / 8);
}

int runeExtractCost(const Item& it) {
    return 10 + it.iLvl;
}

} // namespace rpg