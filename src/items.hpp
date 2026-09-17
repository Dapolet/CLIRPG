#pragma once

#include "bestiary.hpp"
#include "core.hpp"

#include <array>
#include <string>
#include <vector>

namespace rpg {

enum class Rarity      { Common, Uncommon, Rare, Epic, Legendary };
enum class Slot        { Weapon, Armor, Ring, Amulet, Helm, Gloves, Boots, Trinket };
enum class ItemTier    { Iron, Steel, Mythril, Adamant, Void };
enum class PotionKind  { None, Healing, Mana };
enum class RelicPower  { None, Thorns, Potent, GoldenTouch, Dodge, Aegis, Scavenger,
                         kNumRelics };
enum class AffixType   {
    DamagePct, CritChance, CritBonus, LifeSteal,
    MaxHp, Defense, Regen,
    Mana, ManaRegen, XpGain,
};
enum class RuneType    { Power, Vitality, Warding, Flow, Finesse };

// Set bonuses (2-piece / 3-piece) from matching equipped items.
enum class SetId       { None, Titanic, Infernal, Frostbound, Voidwalk, kNumSets };
// Rift Aspect perks — permanent, chosen once per Ascension.
enum class PerkId      { Heirloom, Runeforge, Insight, Vitals, Leeching, Bulwark,
                         Greed, Regeneration, Evasion, Bargain, kNumPerks };

constexpr int kNumSlots  = 8;
constexpr int kNumRarities = 5;
constexpr int kNumTiers  = 5;
constexpr int kNumAffixTypes = 10;

int        slotIndex(Slot s);
const char* rarityName(Rarity r);
const char* slotName(Slot s);
bool        isDefenseSlot(Slot s);           // Armor, Helm, Gloves, Boots
const char* tierName(ItemTier t);
const char* affixName(AffixType t);
const char* runeName(RuneType t);
const char* setName(SetId s);
const char* perkName(PerkId p);
const char* relicPowerName(RelicPower r);
Rarity     rarityByIndex(int i);
ItemTier   tierByIndex(int i);
int        tierUnlockFloor(ItemTier t);

int socketsFor(Rarity r);                 // 0/1/1/2/3

struct Affix {
    std::string prefix;
    AffixType type = AffixType::DamagePct;
    int value = 0;

    bool operator==(const Affix&) const = default;
};

struct Rune {
    RuneType type = RuneType::Power;
    int value = 0;

    bool operator==(const Rune&) const = default;
    std::string describe() const;
};

struct Item {
    std::string name;
    int uid = 0;                   // stable identity (0 = unset)
    Slot slot = Slot::Weapon;
    ItemTier tier = ItemTier::Iron;
    Rarity rarity = Rarity::Common;
    SetId setTag = SetId::None;
    RelicPower relic = RelicPower::None;
    int iLvl = 1;
    int power = 0;                 // weapon attack / armor defense
    std::vector<Affix> affixes;
    std::vector<Rune> runes;
    int extraSockets = 0;          // bonus sockets granted by blacksmith Awaken

    bool consumable = false;
    PotionKind potionKind = PotionKind::None;
    int count = 0;                 // stack size for potions (0 for gear)
    int heal = 0;                  // potions: base display (effect = f(current floor))
    int manaRestore = 0;

    bool isPot() const { return consumable; }
    bool stacks() const { return potionKind != PotionKind::None; }
    int  freeSockets() const { return socketsFor(rarity) + extraSockets - static_cast<int>(runes.size()); }
    bool hasFreeSocket() const { return freeSockets() > 0; }
    std::string describe(int floor = 0) const;

    bool operator==(const Item&) const = default;
};

int sellPrice(const Item& it);                   // 8·iLvl·(rarity+1)/2
int salvageShards(const Item& it);               // 1 + iLvl/20 + 2·rarity
int salvageEssence(const Item& it);              // 1 for Epic/Legendary
int discountedCost(int cost, int discountPct);   // Bargain perk, never below 1

// Potions are floor-agnostic templates: their effect is derived from the
// player's CURRENT floor at use/display time, never stored at creation.
int potionAmount(PotionKind k, int floor);       // heal = 55+22f, mana = 45+14f
int potionHeal(const Item& it, int floor);       // 0 unless Healing kind
int potionMana(const Item& it, int floor);       // 0 unless Mana kind

// The player's persistent stash. Lives across deaths.
struct Vault {
    int gold = 0;
    int shards = 0;
    int essence = 0;

    // records & meta (persisted)
    int bestFloor = 1;
    int bossesSlain = 0;
    int kills = 0;
    int deaths = 0;
    int legendaryFound = 0;
    int mastery = 0;
    int aspect = 0;
    int totalGoldEarned = 0;
    int nextUid = 1;
    std::int64_t saveTime = 0;

    std::vector<Item> items;                    // bag + equipped together
    std::array<int, kNumSlots> equipped = { -1, -1, -1, -1, -1, -1, -1, -1 };  // index into items, -1 = empty
    std::vector<Rune> runes;                    // unbound runestones
    std::array<int, 2> belt = { 0, 0 };         // PotionKind code (+1), 0 = empty
    std::array<bool, static_cast<std::size_t>(PerkId::kNumPerks)> perks{};
    Bestiary bestiary;

    int equippedIndex(Slot s) const;
    void equip(int itemIndex);
    void unequip(Slot s);
    void add(const Item& it);                     // binds a fresh uid when unset
    void addPotion(const Item& it);               // stacks same-kind potions, else adds
    bool usePotion(int idx);                      // consumes one charge, erases stack at 0
    int  indexOfUid(int uid) const;
    int  beltIndex(int n) const;                  // first stack matching bound kind, -1 if none
    PotionKind beltKind(int n) const;
    void bindBelt(int n, PotionKind k);
    bool removeAt(int idx);
    bool hasItem(int idx) const;
    void clear();
};

// Number of equipped items belonging to a set (for 2-piece / 3-piece bonuses).
int setPieces(const Vault& vault, SetId s);

// --- loot generation ---
Item            makePotion(int pact, core::Rng& rng);
Item            makeVendorPotion(int pact, bool mana);
Item            makeGear(int floor, core::Rng& rng, Rarity minRarity = Rarity::Common);
Item            makeTrinket(int floor, core::Rng& rng, Rarity minRarity = Rarity::Common);
SetId           rollSet(int floor, core::Rng& rng);   // floor-unlocked set, or None
std::vector<Item> rollLoot(int floor, bool boss, core::Rng& rng);
Rune            makeRune(int floor, core::Rng& rng);
std::vector<Rune> rollRunestoneLoot(int floor, bool boss, core::Rng& rng, bool boosted = false);

// --- blacksmith ---
bool canUpgrade(const Item& it, int floor);
void upgrade(Item& it);
void reforge(Item& it, core::Rng& rng);
bool canAwaken(const Item& it);
void awaken(Item& it, core::Rng& rng);
bool socketRune(Item& it, const Rune& rune);       // fails when no free socket
Rune extractRune(Item& it);                        // pops the last bound rune
int  reforgeCost(const Item& it);
int  upgradeCost(const Item& it);
int  awakenCost(const Item& it);
int  runeExtractCost(const Item& it);

} // namespace rpg