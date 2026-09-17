#include "character.hpp"
#include "combat.hpp"
#include "core.hpp"
#include "game.hpp"
#include "glory.hpp"
#include "io.hpp"
#include "items.hpp"
#include "save.hpp"
#include "ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <set>
#include <string>
#include <vector>

using namespace rpg;

namespace {

int failures = 0;
int checks = 0;
const char* g_currentTest = "";

#define CHECK(c, msg)                                             \
    do {                                                          \
        ++checks;                                                \
        if (!(c)) {                                               \
            ++failures;                                           \
            std::cout << "FAIL(line " << __LINE__ << "): " << msg \
                      << " [in " << g_currentTest << "]\n";       \
        }                                                         \
    } while (0)

class IoFixture {
public:
    explicit IoFixture(const std::string& input, bool forcePlain = true)
        : oldIn(nullptr), oldOut(nullptr) {
        in.str(input);
        oldIn = std::cin.rdbuf(in.rdbuf());
        oldOut = std::cout.rdbuf(out.rdbuf());
        changedPlain = forcePlain;
        if (forcePlain) ui::setForcePlain(true);
    }
    ~IoFixture() {
        std::cin.clear();
        if (changedPlain) ui::setForcePlain(false);
        std::cin.rdbuf(oldIn);
        std::cout.rdbuf(oldOut);
    }
    IoFixture(const IoFixture&) = delete;
    IoFixture& operator=(const IoFixture&) = delete;
    std::string text() const { return out.str(); }
    bool contains(const std::string& needle) const {
        return out.str().find(needle) != std::string::npos;
    }

private:
    std::stringstream in;
    std::stringstream out;
    std::streambuf* oldIn;
    std::streambuf* oldOut;
    bool changedPlain;
};

std::string repeat(const std::string& s, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) out += s;
    return out;
}

int countSubstr(const std::string& hay, const std::string& needle) {
    int n = 0;
    size_t pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) {
        ++n;
        pos += needle.size();
    }
    return n;
}

std::vector<int> enemyHits(const std::string& text) {
    std::vector<int> out;
    size_t pos = 0;
    while ((pos = text.find("hits you for ", pos)) != std::string::npos) {
        pos += 13;
        const size_t end = text.find_first_of(" \n", pos);
        if (end != std::string::npos) out.push_back(std::atoi(text.substr(pos, end - pos).c_str()));
    }
    return out;
}

std::vector<int> playerStrikes(const std::string& text, const std::string& name) {
    std::vector<int> out;
    const std::string needle = "strike " + name + " for ";
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        pos += needle.size();
        const size_t end = text.find_first_of(" \n", pos);
        if (end != std::string::npos) out.push_back(std::atoi(text.substr(pos, end - pos).c_str()));
    }
    return out;
}

std::vector<int> enemyHpReadouts(const std::string& text, const std::string& name) {
    std::vector<int> out;
    const std::string needle = " HP ";
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        const size_t lineEnd = text.find('\n', pos);
        const size_t lineStart = text.rfind('\n', pos) == std::string::npos ? 0 : text.rfind('\n', pos) + 1;
        const std::string line = text.substr(lineStart, lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart);
        if (line.find(name) != std::string::npos) {
            const size_t slash = line.find('/', pos - lineStart);
            if (slash != std::string::npos) {
                size_t start = slash;
                while (start > 0 && std::isdigit(static_cast<unsigned char>(line[start - 1]))) --start;
                out.push_back(std::atoi(line.substr(start, slash - start).c_str()));
            }
        }
        pos = pos + 4;
    }
    return out;
}

void testRng() {
    core::Rng a(42), b(42);
    CHECK(a.next() == b.next(), "RNG deterministic from same seed");
    CHECK(a.roll(1, 1) == 1, "roll inclusive handle equal bounds");
    int seen = 0;
    for (int i = 0; i < 100; ++i) {
        const int v = a.roll(3, 3);
        if (v != 3) seen++;
    }
    CHECK(seen == 0, "roll(3,3) always 3");
    int lo = 100, hi = 0;
    for (int i = 0; i < 500; ++i) {
        const int v = a.roll(1, 10);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    CHECK(lo == 1 && hi == 10, "roll covers full range over sample");
}

void testFormulas() {
    CHECK(core::xpNeeded(2) > core::xpNeeded(1), "xpNeeded monotonic");
    CHECK(core::xpNeeded(50) > core::xpNeeded(25), "xpNeeded grows");
    CHECK(core::enemyScale(1) == 1.0, "floor 1 scale is 1");
    CHECK(core::enemyScale(2) > 1.0, "scale grows");
    double prev = 0;
    for (int f = 1; f <= 200; ++f) {
        const double s = core::enemyScale(f);
        CHECK(s > prev, "enemyScale strictly increasing");
        prev = s;
    }
    CHECK(core::mitigate(100, 0) == 100.0, "no defense = full damage");
    CHECK(core::mitigate(100, 100) == 50.0, "100 def halves");
    CHECK(std::abs(core::mitigate(100, 300) - 25.0) < 0.001, "300 def = 25% dmg");
    CHECK(core::mitigate(0, 50) == 0.0, "zero damage stays zero");
    CHECK(core::mitigate(50, 100) < core::mitigate(50, 50), "more def = less dmg");
    CHECK(core::critValue(100, 0) == 150.0, "crit base 1.5x");
    CHECK(core::critValue(100, 50) == 200.0, "crit scales with bonus");
}

void testRarityDistribution() {
    core::Rng rng(1234);
    std::set<int> seen;
    for (int i = 0; i < 4000; ++i) {
        const Item it = makeGear(200, rng);
        seen.insert(static_cast<int>(it.rarity));
        CHECK(it.iLvl >= 1, "gear always has positive iLvl");
        CHECK(it.affixes.size() >= 1 && it.affixes.size() <= 4, "affix count in range");
    }
    CHECK(seen.size() == 5, "all rarities appear at high floor");
}

void testReforgeKeepIdentity() {
    core::Rng rng(99);
    for (int i = 0; i < 200; ++i) {
        Item it = makeGear(40, rng);
        const auto slot = it.slot;
        const auto rarity = it.rarity;
        const int ilvl = it.iLvl;
        const int na = static_cast<int>(it.affixes.size());
        const std::string subtype = it.name.substr(it.name.rfind(' ') + 1);
        reforge(it, rng);
        CHECK(it.slot == slot, "reforge keeps slot");
        CHECK(it.rarity == rarity, "reforge keeps rarity");
        CHECK(it.iLvl == ilvl, "reforge keeps iLvl");
        CHECK(static_cast<int>(it.affixes.size()) == na, "reforge keeps affix count");
        CHECK(subtype == it.name.substr(it.name.rfind(' ') + 1), "reforge keeps gear type");
        CHECK(!it.name.empty(), "reforge regens name");
    }
}

void testGearTypeVariety() {
    core::Rng rng(777);
    std::set<std::string> weapons, armor;
    for (int i = 0; i < 800; ++i) {
        const Item it = makeGear(200, rng);
        if (it.slot == Slot::Weapon) {
            CHECK(it.name.find(" Blade") == std::string::npos, "weapon uses a real weapon type");
            weapons.insert(it.name.substr(it.name.rfind(' ') + 1));
        } else if (it.slot == Slot::Armor) {
            CHECK(it.name.find(" Armor") == std::string::npos, "armor uses a real armor type");
            armor.insert(it.name.substr(it.name.rfind(' ') + 1));
        }
    }
    CHECK(weapons.size() >= 8, "at least 8 weapon types appear");
    CHECK(armor.size() >= 8, "at least 8 armor types appear");
}

void testUpgradeCap() {
    core::Rng rng(5);
    Item it = makeGear(50, rng);
    it.iLvl = 3;
    it.power = 10;
    it.slot = Slot::Weapon;
    CHECK(canUpgrade(it, 10), "upgrade allowed below floor cap");
    upgrade(it);
    CHECK(it.iLvl == 4, "upgrade bumps iLvl");
    CHECK(it.power > 10, "upgrade bumps weapon power");
    CHECK(!canUpgrade(it, 4), "upgrade blocked at cap");
}

void testAwaken() {
    core::Rng rng(5);
    Item rare;
    rare.consumable = false;
    rare.rarity = Rarity::Rare;
    rare.affixes.push_back(Affix{ "Keen", AffixType::CritChance, 2 });
    rare.affixes.push_back(Affix{ "Fierce", AffixType::DamagePct, 5 });
    CHECK(rare.freeSockets() == 1, "rare base socket");
    CHECK(canAwaken(rare), "full-affix rare can awaken");
    awaken(rare, rng);
    CHECK(rare.extraSockets == 1, "awaken opens a bonus socket");
    CHECK(rare.freeSockets() == 2, "awakened rare has two sockets");
    CHECK(!canAwaken(rare), "an awakened item cannot awaken twice");

    Item epic;
    epic.consumable = false;
    epic.rarity = Rarity::Epic;
    epic.affixes.push_back(Affix{ "Brilliant", AffixType::Mana, 8 });
    awaken(epic, rng);
    CHECK(epic.freeSockets() == 3, "epic awakens to three sockets");

    Item common;
    common.rarity = Rarity::Common;
    common.affixes.push_back(Affix{ "Sturdy", AffixType::MaxHp, 4 });
    CHECK(!canAwaken(common), "common gear never awakens");
}

void testSpellTree() {
    for (auto cls : { ClassId::Warrior, ClassId::Mage, ClassId::Rogue, ClassId::Paladin }) {
        Character pc(cls);
        CHECK(classSpells(cls).size() == 12, "each class has 12 spells");
        CHECK(pc.pointsSpent() == 0, "fresh tree costs nothing");
        CHECK(pc.spendPoint(0, 2) == false, "can't skip depth without prereq");
        CHECK(pc.spendPoint(0, 3) == false, "can't skip depth3 without prereq");
        {
            bool hasDebuff = false;
            for (const auto& sp : classSpells(cls))
                if (sp.enemyAtkDownPct > 0 || sp.enemyVulnPct > 0) hasDebuff = true;
            CHECK(hasDebuff, "each class has an Enfeeble/Vulnerable debuff spell");
        }
        CHECK(pc.spendPoint(0, 0), "depth0 unlockable");
        CHECK(pc.spendPoint(0, 0) == false, "can't unlock twice");
        CHECK(pc.spendPoint(0, 1) == false, "need 2 points for depth1");
        CHECK(pc.spell(0, 0).name == classSpells(cls)[0].name, "spell lookup matches");
        pc.respec();
        CHECK(pc.pointsSpent() == 0, "respec clears the tree");
        CHECK(pc.skillPoints() == 2, "respec refunds points");
    }
}

void testStatsGear() {
    core::Rng rng(77);
    Character pc(ClassId::Warrior);
    Vault v;
    const auto base = pc.stats(v);
    Item sword = makeGear(30, rng);
    sword.slot = Slot::Weapon;
    sword.power = 12;
    v.items.push_back(sword);
    v.equip(0);
    const auto with = pc.stats(v);
    CHECK(with.attack >= base.attack + 12, "weapon power feeds attack");
    CHECK(v.equippedIndex(Slot::Weapon) == 0, "equip stores index");
    v.unequip(Slot::Weapon);
    CHECK(pc.stats(v).attack == base.attack, "unequip removes bonus");
}

void testPotion() {
    core::Rng rng(8);
    for (int f = 1; f <= 60; ++f) {
        const Item p = makePotion(f, rng);
        CHECK(p.isPot(), "potion flagged consumable");
        CHECK(p.heal > 0 || p.manaRestore > 0, "potion does something");
        CHECK(!p.name.empty(), "potion has name");
    }
}

void testSaveRoundtrip() {
    core::Rng rng(2024);
    const std::string path = "test_save.rpg";
    Character pc(ClassId::Rogue);
    pc.gainXp(1'000, 0);
    pc.setFloor(3);
    CHECK(pc.spendPoint(0, 0), "seed a spell");

    Vault v;
    v.gold = 123;
    v.shards = 4;
    v.essence = 2;
    Item gear = makeGear(5, rng);
    v.items.push_back(gear);
    v.equip(0);
    v.items.push_back(makePotion(3, rng));
    Item awakened = makeGear(20, rng);
    awakened.extraSockets = 1;
    v.items.push_back(awakened);

    CHECK(save::write(path, pc, v), "write succeeds");
    Character pc2(ClassId::Warrior);
    Vault v2;
    CHECK(save::read(path, &pc2, &v2), "read succeeds");

    const auto s1 = pc.snapshot();
    const auto s2 = pc2.snapshot();
    CHECK(s2.id == s1.id, "roundtrip class");
    CHECK(s2.level == s1.level, "roundtrip level");
    CHECK(s2.xp == s1.xp, "roundtrip xp");
    CHECK(s2.skillPoints == s1.skillPoints, "roundtrip skill points");
    CHECK(s2.floor == s1.floor, "roundtrip floor");
    CHECK(s2.unlocked == s1.unlocked, "roundtrip spell tree");
    CHECK(v2.items.size() == v.items.size(), "roundtrip item count");
    if (v2.items.size() == v.items.size()) {
        for (std::size_t i = 0; i < v.items.size(); ++i)
            CHECK(v2.items[i] == v.items[i], "roundtrip item equality");
    }
    CHECK(v2.equipped == v.equipped, "roundtrip equipped");
    CHECK(v2.gold == v.gold && v2.shards == v.shards && v2.essence == v.essence,
          "roundtrip materials/gold");
    CHECK(v2.saveTime > 0, "saveTime is stamped on write");
    CHECK(v2.saveTime == v.saveTime, "saveTime roundtrips exactly");
    CHECK(v2.items.back().extraSockets == 1, "awakened socket survives save/load");
    CHECK(v2.items.back() == v.items.back(), "awakened item roundtrips equal");
    CHECK(v2.removeAt(0) == true, "removeAt works post-load");
    CHECK(v2.equipped[slotIndex(Slot::Weapon)] == -1, "removing unpins equipped");

    std::remove(path.c_str());
}

void testSaveCorrupt() {
    const std::string path = "test_corrupt.rpg";
    Character pc(ClassId::Warrior);
    Vault v;
    CHECK(save::write(path, pc, v), "seed corrupt file");
    {
        std::ifstream in(path);
        std::string all((std::istreambuf_iterator<char>(in)), {});
        // corrupt the checksum value right after the [sig] tag
        const auto start = all.rfind("[sig]\n");
        const auto nl = all.find('\n', start + 6);
        const int sig = std::atoi(all.substr(start + 6, nl - start - 6).c_str());
        all.replace(start + 6, nl - start - 6, std::to_string(sig + 1));
        std::ofstream out(path, std::ios::trunc);
        out << all;
    }
    Character pc2(ClassId::Warrior);
    Vault v2;
    CHECK(!save::read(path, &pc2, &v2), "checksum mismatch rejected");

    // wrong version header
    CHECK(save::write(path, pc, v), "re-seed file");
    {
        std::ifstream in(path);
        std::string all((std::istreambuf_iterator<char>(in)), {});
        all.replace(0, all.find('\n'), "RPGSAVE v1");
        std::ofstream out(path, std::ios::trunc);
        out << all;
    }
    CHECK(!save::read(path, &pc2, &v2), "unknown version rejected");
    std::remove(path.c_str());
}

void testEnemyGen() {
    core::Rng rng(31);
    for (int f = 1; f <= 50; ++f) {
        const auto enc = combat::makeEncounter(f, rng);
        CHECK(!enc.empty(), "encounter never empty");
        for (const auto& e : enc) {
            CHECK(e.alive(), "enemies spawn alive");
            CHECK(e.attack >= 1, "enemies deal damage");
            CHECK(e.xpReward >= 1, "enemies grant xp");
        }
        const combat::Enemy boss = combat::makeBoss(f, rng);
        CHECK(boss.boss, "makeBoss is a boss");
        CHECK(boss.hpMax > 0, "boss has hp");
        CHECK(boss.xpReward >= 1, "boss grants xp");
    }
}

void testAffixDistinctness() {
    core::Rng rng(17);
    for (int i = 0; i < 1000; ++i) {
        const Item it = makeGear(10, rng);
        std::set<int> types;
        for (const auto& a : it.affixes) {
            CHECK(types.insert(static_cast<int>(a.type)).second,
                  "affix types never duplicate on an item");
        }
    }
}

void testDeathPreservesVault() {
    core::Rng rng(41);
    Character pc(ClassId::Mage);
    pc.setFloor(4);
    Vault v;
    v.gold = 100;
    v.shards = 6;
    v.essence = 3;
    Item gear;
    gear.slot = Slot::Weapon;
    gear.tier = ItemTier::Iron;
    gear.rarity = Rarity::Common;
    gear.iLvl = 1;
    gear.power = 3;
    gear.name = "Iron Blade";
    v.items.push_back(gear);
    v.equip(0);
    const Item lucky = makePotion(2, rng);
    v.items.push_back(lucky);

    const auto itemsBefore = v.items;

    save::applyDeath(pc, v);
    CHECK(v.items == itemsBefore, "death keeps every vaulted item");
    CHECK(v.gold == 80, "death strips exactly 20% gold");
    CHECK(v.shards == 6 && v.essence == 3, "death keeps materials");
    CHECK(pc.currentFloor() == 3, "death drops one floor");
    CHECK(pc.hp() == pc.stats(v).maxHp, "death fully restores HP");
    CHECK(pc.resource() == pc.stats(v).maxResource, "death fully restores resource");
    CHECK(pc.level() == 1, "death keeps level");

    Character pc2(ClassId::Mage);
    pc2.setFloor(1);
    Vault v2;
    v2.gold = 9;
    save::applyDeath(pc2, v2);
    CHECK(pc2.currentFloor() == 1, "death never drops below floor 1");
}

void testDeathByFight() {
    core::Rng rng(50);
    Character pc(ClassId::Mage);
    Vault v;
    v.gold = 77;
    v.items.push_back(makePotion(1, rng));

    using namespace combat;
    Enemy killer;
    killer.name = "Killer";
    killer.hpMax = killer.hp = 5000;
    killer.attack = 5000;
    killer.defense = 0;
    killer.xpReward = 999;
    killer.goldReward = 999;

    std::istringstream in("a\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const Result res = fight(pc, v, { killer }, rng);
    std::cin.rdbuf(oldIn);

    CHECK(!res.won, "overwhelming enemy kills player");
    CHECK(v.items.size() == 1, "player death does not consume vault from quest loot");
    CHECK(pc.hp() == 0, "player at zero hp after death");
    CHECK(v.gold == 77, "fight loot only applies on victory");
}

void testThreatScalingGentle() {
    // v5 curve: kGrowRate 1.07 instead of 1.12
    CHECK(core::kGrowRate == 1.07, "growth rate gentler at 1.07");
    double prev = 0;
    for (int f = 1; f <= 200; ++f) {
        const double s = core::enemyScale(f);
        CHECK(s > prev, "enemyScale strictly increasing");
        prev = s;
    }
    // deep climb is ~1.07^50 = 29x over fifty floors (1.12^50 was 289x)
    const double ratio = core::enemyScale(75) / core::enemyScale(25);
    CHECK(ratio > 28 && ratio < 31, "deep stairs stay climbable (was 289x)");
    CHECK(core::aspectScale(0) == 1.0, "aspect 0 is baseline");
    CHECK(core::aspectScale(1) == 1.15, "aspect 1 = +15%");
    CHECK(core::aspectScale(4) == 1.6, "aspect 4 stacks");
}

void testElementTable() {
    using E = core::Element;
    CHECK(core::elementMult(E::None, E::Fire) == 1.0, "neutral player hits elemental");
    CHECK(core::elementMult(E::Fire, E::None) == 1.0, "elemental player hits neutral");
    CHECK(core::elementMult(E::None, E::None) == 1.0, "neutral vs neutral");

    CHECK(core::elementMult(E::Fire, E::Fire)   == 0.7, "same element resists");
    CHECK(core::elementMult(E::Frost, E::Frost) == 0.7, "same element resists");
    CHECK(core::elementMult(E::Arcane, E::Arcane) == 0.7, "same element resists");

    CHECK(core::elementMult(E::Fire, E::Frost)   == 1.3, "Fire counters Frost");
    CHECK(core::elementMult(E::Frost, E::Arcane) == 1.3, "Frost counters Arcane");
    CHECK(core::elementMult(E::Arcane, E::Fire)  == 1.3, "Arcane counters Fire");

    CHECK(core::elementMult(E::Fire, E::Arcane)   == 1.0, "Fire vs Arcane neutral");
    CHECK(core::elementMult(E::Frost, E::Fire)    == 1.0, "Frost vs Fire neutral");

    // mage spells carry their branch element
    const auto& spells = classSpells(ClassId::Mage);
    for (int i = 0; i < 4; ++i)  CHECK(spells[static_cast<std::size_t>(i)].element == E::Fire,   "fire branch");
    for (int i = 4; i < 8; ++i)  CHECK(spells[static_cast<std::size_t>(i)].element == E::Frost,  "frost branch");
    for (int i = 8; i < 12; ++i) CHECK(spells[static_cast<std::size_t>(i)].element == E::Arcane, "arcane branch");
}

void testRunestoneHelpers() {
    // sockets: 0/1/1/2/3
    CHECK(socketsFor(Rarity::Common) == 0, "common 0 sockets");
    CHECK(socketsFor(Rarity::Uncommon) == 1, "uncommon 1 socket");
    CHECK(socketsFor(Rarity::Rare) == 1, "rare 1 socket");
    CHECK(socketsFor(Rarity::Epic) == 2, "epic 2 sockets");
    CHECK(socketsFor(Rarity::Legendary) == 3, "legendary 3 sockets");

    Item epic;
    epic.rarity = Rarity::Epic;
    epic.consumable = false;
    epic.iLvl = 20;
    epic.name = "Test";
    CHECK(epic.freeSockets() == 2, "epic starts with 2 free sockets");

    Rune r;
    r.type = RuneType::Power;
    r.value = 8;
    CHECK(socketRune(epic, r), "socket into free slot works");
    CHECK(epic.runes.size() == 1, "rune bound");
    CHECK(epic.freeSockets() == 1, "one socket consumed");
    CHECK(socketRune(epic, r), "second socket works");
    CHECK(socketRune(epic, r) == false, "no free sockets => fail");
    CHECK(epic.runes.size() == 2, "overflow rejected");

    const Rune popped = extractRune(epic);
    CHECK(popped.type == RuneType::Power && popped.value == 8, "extract pops last rune");
    CHECK(epic.runes.size() == 1, "extract shrinks socket list");
    CHECK(runeExtractCost(epic) == 30, "extract costs 10 + iLvl");

    Item pot;
    pot.consumable = true;
    CHECK(socketRune(pot, r) == false, "potions cannot be socketed");

    // drop lag: makeGear(30) iLvl never above floor-1 lower bound beyond the 6 cap
    core::Rng rng(11);
    for (int i = 0; i < 100; ++i) {
        const Item g = makeGear(30, rng);
        CHECK(g.iLvl >= 24 && g.iLvl <= 30, "drop iLvl lags floor by <= min(floor/4,6)");
    }
}

void testRunestoneEconomy() {
    Item rare;
    rare.rarity = Rarity::Rare;
    rare.consumable = false;
    rare.iLvl = 20;
    CHECK(sellPrice(rare) == 8 * 20 * 3 / 2, "sell price = 8*iLvl*(rarity+1)/2");
    CHECK(salvageShards(rare) == 1 + 1 + 4, "salvage shards = 1 + iLvl/20 + 2*rarity");
    CHECK(salvageEssence(rare) == 0, "rare salvages no essence");

    Item epic;
    epic.rarity = Rarity::Epic;
    epic.consumable = false;
    epic.iLvl = 40;
    CHECK(salvageEssence(epic) == 1, "epic gives essence");

    Item pot;
    pot.consumable = true;
    CHECK(sellPrice(pot) == 0, "potions unsellable");
    CHECK(salvageShards(pot) == 0, "potions unsalvageable");

    core::Rng rng(3);
    const Rune rune = makeRune(50, rng);
    CHECK(rune.value >= 1, "rune values always positive");
    CHECK(static_cast<int>(rune.type) >= 0 && static_cast<int>(rune.type) <= 4, "valid rune type");
    CHECK(rollRunestoneLoot(20, true, rng).size() == 1, "boss always drops a runestone");
    for (int i = 0; i < 200; ++i)
        CHECK(rollRunestoneLoot(5, false, rng).size() <= 1, "normal fight never drops 2 runes");
}

void testMasteryCompounding() {
    Character pc(ClassId::Warrior);
    pc.gainXp(1'000'000, 0);              // high level so +8% is a visible integer shift
    while (pc.level() < 10) pc.gainXp(1'000'000, 0);
    Vault v0;
    const auto base = pc.stats(v0);

    Vault v1 = v0; v1.mastery = 1;
    Vault v2 = v0; v2.mastery = 3;
    const auto s1 = pc.stats(v1);
    const auto s2 = pc.stats(v2);

    CHECK(s1.attack > base.attack, "mastery 1 boosts attack");
    CHECK(s1.maxHp > base.maxHp, "mastery 1 boosts max HP");
    CHECK(s1.maxResource > base.maxResource, "mastery 1 boosts resource");
    CHECK(s2.attack > s1.attack, "mastery compounds");
    CHECK(s2.maxHp > s1.maxHp, "mastery compounds HP");
    // 1.08^1 on base attack
    CHECK(s1.attack == static_cast<int>(static_cast<double>(base.attack) * 1.08),
          "attack multiplier is exactly 1.08^mastery");

    // effective caps: heal/restore respect compounding maxima
    pc.healHp(9999, s2.maxHp);
    CHECK(pc.hp() <= s2.maxHp, "heal respects mastery-scaled cap");
    pc.restoreResource(9999, s2.maxResource);
    CHECK(pc.resource() <= s2.maxResource, "resource respects mastery-scaled cap");
    pc.restoreAll(s2.maxHp, s2.maxResource);
    CHECK(pc.hp() == s2.maxHp && pc.resource() == s2.maxResource, "restoreAll honours caps");
}

void testMasteryRunestones() {
    Character pc(ClassId::Warrior);
    Vault v;
    Item sword;
    sword.slot = Slot::Weapon;
    sword.rarity = Rarity::Epic;
    sword.power = 10;
    v.items.push_back(sword);
    v.equip(0);
    const auto before = pc.stats(v);

    Rune power;  power.type  = RuneType::Power;    power.value  = 10;
    Rune vit;    vit.type    = RuneType::Vitality; vit.value    = 20;
    Rune ward;   ward.type   = RuneType::Warding;  ward.value   = 15;
    Rune flow;   flow.type   = RuneType::Flow;     flow.value   = 2;
Rune fin;    fin.type   = RuneType::Finesse;  fin.value    = 3;
    socketRune(v.items[0], power);
    socketRune(v.items[0], vit);
    const auto after = pc.stats(v);

    CHECK(after.attack > before.attack, "Power rune raises attack");
    CHECK(after.maxHp > before.maxHp, "Vitality rune raises max HP");
}

void testHealScaling() {
    const auto& bastion = classSpells(ClassId::Warrior)[6];
    const auto& rally = classSpells(ClassId::Warrior)[9];
    CHECK(spellHeal(bastion, 1, 1) == 10 + 1, "Bastion scales 1.2/level");
    CHECK(spellHeal(bastion, 10, 1) == 10 + 12, "Bastion scales 1.2/level");
    CHECK(spellHeal(rally, 1, 1) == 8 + 1, "Rally scales 1.0/level");
    CHECK(spellHeal(rally, 10, 1) == 8 + 10, "Rally scales 1.0/level");
    const int baseHeal = spellHeal(rally, 10, 1);
    CHECK(spellHeal(rally, 10, 5) > baseHeal, "Heals scale with floor");

    Character pc(ClassId::Warrior);
    Vault v;
    const auto st = pc.stats(v);
    pc.takeDamage(5);
    pc.healHp(3, st.maxHp);
    CHECK(pc.hp() == st.maxHp - 2, "healHp applies with cap");
    pc.takeDamage(50);
    pc.healHp(9999, st.maxHp);
    CHECK(pc.hp() == st.maxHp, "healHp never overheals");
}

void testAspectScaling() {
    core::Rng a(2024), b(2024);
    const auto low = combat::makeEncounter(10, a, 0);
    const auto high = combat::makeEncounter(10, b, 2);
    CHECK(low.size() == high.size(), "same seed, same enemy picks");
    for (std::size_t i = 0; i < low.size() && i < high.size(); ++i) {
        CHECK(high[i].hpMax > low[i].hpMax, "aspect raises enemy HP");
        CHECK(high[i].attack > low[i].attack, "aspect raises enemy ATK");
    }
    const auto bl = combat::makeBoss(20, a, 0);
    const auto bh = combat::makeBoss(20, b, 2);
    CHECK(bh.hpMax > bl.hpMax && bh.attack > bl.attack, "aspect raises boss threat");
}

void testBossLootEpic() {
    core::Rng rng(88);
    // boss fight: guaranteed epic+ piece
    for (int i = 0; i < 40; ++i) {
        const auto items = rollLoot(30, true, rng);
        CHECK(items.size() == 3, "boss drops two pieces and a trinket");
        bool hasEpicPlus = false;
        for (const auto& it : items)
            if (it.rarity >= Rarity::Epic) hasEpicPlus = true;
        CHECK(hasEpicPlus, "boss loot always contains epic+ item");
    }
}

void testSaveV4RecordsAndRunes() {
    core::Rng rng(5);
    const std::string path = "test_v4.rpg";
    Character pc(ClassId::Mage);
    pc.setFloor(42);

    Vault v;
    v.gold = 100;
    v.bestFloor = 60;
    v.bossesSlain = 3;
    v.kills = 120;
    v.deaths = 2;
    v.legendaryFound = 1;
    v.mastery = 2;
    v.aspect = 1;

    Item gear;
    gear.slot = Slot::Weapon;
    gear.rarity = Rarity::Epic;
    gear.iLvl = 25;
    gear.power = 30;
    gear.name = "Rune Blade";
    Rune power; power.type = RuneType::Power; power.value = 9;
    Rune fin;   fin.type   = RuneType::Finesse; fin.value = 4;
    socketRune(gear, power);
    socketRune(gear, fin);
    v.items.push_back(gear);
    v.equip(0);
    v.runes.push_back(Rune{ RuneType::Vitality, 12 });
    v.runes.push_back(Rune{ RuneType::Flow, 1 });

    CHECK(save::write(path, pc, v), "v4 write succeeds");
    Character pc2(ClassId::Warrior);
    Vault v2;
    CHECK(save::read(path, &pc2, &v2), "v4 read succeeds");
    CHECK(v2.bestFloor == 60 && v2.bossesSlain == 3 && v2.kills == 120, "records roundtrip");
    CHECK(v2.deaths == 2 && v2.legendaryFound == 1, "records roundtrip (2)");
    CHECK(v2.mastery == 2 && v2.aspect == 1, "mastery/aspect roundtrip");
    CHECK(pc2.currentFloor() == 42, "floor roundtrip");
    CHECK(v2.items.size() == 1 && v2.items[0].runes.size() == 2, "socketed runes roundtrip");
    CHECK(v2.runes.size() == 2, "vault runes roundtrip");

    const auto s = save::peek(path);
    CHECK(s.valid, "peek sees existing save");
    CHECK(s.cls == ClassId::Mage && s.level == 1 && s.floor == 42, "peek summary fields");
    CHECK(s.saveTime > 0, "peek exposes stamped saveTime");
    std::remove(path.c_str());

    CHECK(!save::peek("no_such_file.rpg").valid, "peek on missing file invalid");
}

void testBasicAttackLoop() {
    using namespace combat;
    core::Rng rng(1);
    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());

    {
        Character pc(ClassId::Rogue);
        Vault v;
        out.str("");
        Enemy weak;
        weak.name = "Weakling";
        weak.hpMax = weak.hp = 5;
        weak.attack = 1;
        weak.xpReward = 1;
        weak.goldReward = 1;
        std::istringstream in("a\na\na\na\na\na\na\na\na\na\n");
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        const Result res = fight(pc, v, { weak }, rng);
        std::cin.rdbuf(oldIn);
        std::cin.clear();
        CHECK(res.won, "repeated basic attacks finish an easy fight");
        CHECK(pc.hp() == pc.stats(v).maxHp, "rogue untouchable in a trivial fight");
    }

    {
        Character pc(ClassId::Rogue);
        Vault v;
        out.str("");
        Enemy weak;
        weak.name = "Pebble";
        weak.hpMax = weak.hp = 5;
        weak.attack = 1;
        weak.xpReward = 1;
        weak.goldReward = 1;
        std::istringstream in("a\n");
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        const Result res = fight(pc, v, { weak }, rng);
        std::cin.rdbuf(oldIn);
        std::cin.clear();
        CHECK(res.won, "EOF falls back to basic attacks and still wins");
    }

    std::cout.rdbuf(oldOut);
}

void testRogueFirstStrikeSmoke() {
    using namespace combat;
    core::Rng rng(2);
    Character pc(ClassId::Rogue);
    Vault v;
    Enemy boss;
    boss.name = "Boss";
    boss.boss = true;
    boss.hpMax = boss.hp = 1;
    boss.attack = 100000;
    boss.xpReward = 5;
    boss.goldReward = 5;

    std::istringstream in("a\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const Result res = fight(pc, v, { boss }, rng);
    std::cin.rdbuf(oldIn);

    CHECK(res.won, "rogue drops a one-hp boss in the opening round");
    CHECK(pc.hp() == pc.stats(v).maxHp, "boss never gets a turn");
    CHECK(v.kills == 0, "statistics only applied on death, not on fight()");
    CHECK(res.kills == 1, "result records the kill");
}

void testBossSummonSafe() {
    using namespace combat;
    core::Rng rng(1234);
    Character pc(ClassId::Warrior);
    Vault v;
    Item sword;
    sword.slot = Slot::Weapon;
    sword.power = 50;
    v.add(sword);
    v.equip(0);

    Enemy boss;
    boss.name = "Summon Test Boss";
    boss.boss = true;
    boss.hpMax = boss.hp = 100;
    boss.attack = 2;
    boss.xpReward = 1;
    boss.goldReward = 1;

    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream in("a\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const Result res = fight(pc, v, { boss }, rng);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    CHECK(res.won, "boss plus summoned thrall is beatable");
    CHECK(res.kills == 2, "boss and thrall both die");
    const std::string text = out.str();
    CHECK(text.find("summons a Dark Thrall") != std::string::npos, "thrall summon triggers");
    CHECK(text.find("Summon Test Boss hits you") != std::string::npos,
          "boss keeps its name after summoning (no dangling reference)");
    CHECK(text.find("  hits you for") == std::string::npos,
          "no empty-name enemy attack after summon");
    std::size_t thrallHits = 0;
    const std::string thrallAttack = "Dark Thrall hits you for";
    std::size_t pos2 = 0;
    while ((pos2 = text.find(thrallAttack, pos2)) != std::string::npos) {
        ++thrallHits;
        pos2 += thrallAttack.size();
    }
    CHECK(thrallHits == 1, "thrall skips the summon round, then acts on the next round");
}

void testDotExpiry() {
    using namespace combat;
    core::Rng rng(777);
    Character pc(ClassId::Mage);
    CHECK(pc.spendPoint(0, 0), "mage opens with Firebolt");
    Vault v;

    Enemy goo;
    goo.name = "Goo";
    goo.hpMax = goo.hp = 30;
    goo.attack = 1;
    goo.xpReward = 1;
    goo.goldReward = 1;

    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream in("s\n1\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const Result res = fight(pc, v, { goo }, rng);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    CHECK(res.won, "mage outlasts the slime");
    const std::string text = out.str();
    std::size_t ticks = 0, pos = 0;
    while ((pos = text.find("Burn bites Goo", pos)) != std::string::npos) {
        ++ticks;
        pos += std::string("Burn bites Goo").size();
    }
    CHECK(ticks == 2, "2-turn Burn ticks exactly twice then expires");
}

void testStunSkipsEnemyTurn() {
    using namespace combat;
    core::Rng rng(0);
    Character pc(ClassId::Warrior);
    CHECK(pc.spendPoint(1, 0), "warrior unlocks Shield Slam");
    Vault v;

    Enemy lump;
    lump.name = "Lump";
    lump.hpMax = lump.hp = 80;
    lump.attack = 1;
    lump.xpReward = 1;
    lump.goldReward = 1;

    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream in("s\n1\na\ns\n1\na\ns\n1\na\ns\n1\na\ns\n1\na\ns\n1\na\n"
                          "s\n1\na\ns\n1\na\ns\n1\na\ns\n1\na\ns\n1\na\ns\n1\na\n"
                          "s\n1\na\ns\n1\na\ns\n1\na\ns\n1\na\ns\n1\na\ns\n1\na\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const Result res = fight(pc, v, { lump }, rng);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    CHECK(res.won, "stun-heavy fight still finishes");
    std::size_t stuns = 0, pos = 0;
    const std::string needle = "is stunned and skips its turn.";
    while ((pos = out.str().find(needle, pos)) != std::string::npos) {
        ++stuns;
        pos += needle.size();
    }
    CHECK(stuns >= 1, "a stunned enemy visibly skips its turn (stun survives its own round)");
}

void testLevelUpPreservesHp() {
    Character pc(ClassId::Warrior);
    Vault v;
    v.perks[static_cast<std::size_t>(PerkId::Vitals)] = true;  // +8% max HP
    const auto st = pc.stats(v);
    pc.restoreAll(st.maxHp, st.maxResource);
    const int hpBefore = pc.hp();
    const int resBefore = pc.resource();
    pc.gainXp(pc.xpToNext(), 0);
    CHECK(pc.level() == 2, "one level gained");
    CHECK(pc.hp() >= hpBefore, "level-up never drops HP below its pre-level value");
    CHECK(pc.resource() >= resBefore, "level-up never drops resource below its pre-level value");
    CHECK(pc.hp() > 0, "never dead after a level");
}

void testXpAppliedOnce() {
    using namespace combat;
    core::Rng rng(2026);
    Character pc(ClassId::Warrior);
    Vault v;
    v.perks[static_cast<std::size_t>(PerkId::Insight)] = true;

    Enemy rat;
    rat.name = "Rift Rat";
    rat.hpMax = rat.hp = 3;
    rat.attack = 1;
    rat.xpReward = 10;
    rat.goldReward = 1;

    std::istringstream in("a\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const Result res = fight(pc, v, { rat }, rng);
    std::cin.rdbuf(oldIn);
    std::cin.clear();

    CHECK(res.won, "trivial fight is won");
    CHECK(res.xp == 10, "combat reports raw XP, not pre-boosted");
    const int pct = pc.stats(v).xpGainPct;
    CHECK(pct == 10, "Insight perk grants +10% XP");
    pc.gainXp(res.xp, pct);
    CHECK(pc.xp() == 11, "XP bonus applied exactly once (no double-dip)");
}

void testVendorPotion() {
    for (int f = 1; f <= 60; ++f) {
        const Item heal = makeVendorPotion(f, false);
        CHECK(heal.isPot(), "vendor healing draught is a potion");
        CHECK(heal.heal > 0 && heal.manaRestore == 0,
              "healing draught always heals and never restores resource");
        const Item mana = makeVendorPotion(f, true);
        CHECK(mana.isPot(), "vendor mana draught is a potion");
        CHECK(mana.manaRestore > 0 && mana.heal == 0,
              "mana draught always restores resource and never heals");
    }
}

void testEnemyHitCap() {
    using namespace combat;
    core::Rng rng(3);
    Character pc(ClassId::Warrior);
    Vault v;
    const int maxHp = pc.stats(v).maxHp;
    Enemy brute;
    brute.name = "Brute";
    brute.hpMax = brute.hp = 1000;
    brute.attack = 100000;
    brute.xpReward = 1;
    brute.goldReward = 1;

    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream in("a\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    (void)fight(pc, v, { brute }, rng);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    int worst = 0;
    std::string::size_type at = 0;
    while ((at = out.str().find("hits you for ", at)) != std::string::npos) {
        at += 13;
        const std::string::size_type start = at;
        while (at < out.str().size() && out.str()[at] >= '0' && out.str()[at] <= '9') ++at;
        worst = std::max(worst, std::atoi(out.str().substr(start, at - start).c_str()));
    }
    const int cap = std::max(1, maxHp * 60 / 100);
    CHECK(worst == cap, "enemy hits capped at 60% of max HP");
    CHECK(!pc.alive(), "capped hits still eventually spell death");
}

void testPassiveRegenApplied() {
    using namespace combat;
    core::Rng rng(4);
    Character pc(ClassId::Warrior);
    CHECK(pc.train(TrainId::Fleetness), "fleetness rises to rank 1");
    CHECK(pc.train(TrainId::Fleetness), "fleetness rises to rank 2");
    Vault v;
    CHECK(pc.stats(v).regenPerTurn == 2, "fleetness feeds effective regen per turn");
    pc.takeDamage(pc.stats(v).maxHp - 35);

    Enemy dummy;
    dummy.name = "Training Dummy";
    dummy.hpMax = dummy.hp = 10;
    dummy.attack = 0;
    dummy.xpReward = 0;
    dummy.goldReward = 0;

    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream in("a\na\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const Result res = fight(pc, v, { dummy }, rng);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    CHECK(res.won, "two swings fell the training dummy");
    CHECK(pc.hp() == 36, "passive regen heals after the enemy phase (2 regen - 1 min hit)");
}

void testMerchantNoFreeMaterials() {
    core::Rng rng(11);
    Character pc(ClassId::Warrior);
    {
        Vault v;
        v.gold = 5;
        std::ostringstream out;
        std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
        std::istringstream in("3\n4\n0\n");
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        ui::merchantMenu(pc, v, rng);
        std::cin.rdbuf(oldIn);
        std::cin.clear();
        std::cout.rdbuf(oldOut);
        CHECK(v.shards == 0, "failed shard purchase grants no shards");
        CHECK(v.essence == 0, "failed essence purchase grants no essence");
        CHECK(v.gold == 5, "failed purchases spend no gold");
    }
    {
        Vault v;
        v.gold = 50;
        std::ostringstream out;
        std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
        std::istringstream in("3\n0\n");
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        ui::merchantMenu(pc, v, rng);
        std::cin.rdbuf(oldIn);
        std::cin.clear();
        std::cout.rdbuf(oldOut);
        CHECK(v.shards == 3, "successful purchase grants three shards");
        CHECK(v.gold == 38, "successful purchase spends twelve gold");
    }
}

void testSetDropsReachable() {
    core::Rng rng(9);
    bool sawSet = false;
    for (int i = 0; i < 200; ++i)
        for (const auto& it : rollLoot(30, true, rng))
            if (it.setTag != SetId::None) sawSet = true;
    CHECK(sawSet, "bosses can drop set pieces");

    Vault v;
    Item a = makeGear(30, rng);
    a.slot = Slot::Weapon;
    a.power = 0;
    a.setTag = SetId::Titanic;
    a.affixes.clear();
    a.runes.clear();
    Item b = makeGear(30, rng);
    b.slot = Slot::Armor;
    b.power = 0;
    b.setTag = SetId::Titanic;
    b.affixes.clear();
    b.runes.clear();
    v.add(a);
    v.equip(0);
    v.add(b);
    v.equip(1);
    Character pc(ClassId::Warrior);
    const int base = pc.stats(Vault{}).maxHp;
    CHECK(setPieces(v, SetId::Titanic) == 2, "two titanic pieces counted");
    CHECK(pc.stats(v).maxHp == base + base * 15 / 100, "titanic 2-piece raises max HP by 15%");
}

void testDoTFinishTakesVictory() {
    using namespace combat;
    core::Rng rng(777);
    Character pc(ClassId::Mage);
    CHECK(pc.spendPoint(0, 0), "mage opens with Firebolt");
    Vault v;

    Enemy goo;
    goo.name = "Goo";
    goo.hpMax = goo.hp = 6;
    goo.attack = 0;
    goo.xpReward = 0;
    goo.goldReward = 0;

    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream in("s\n1\nf\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const Result res = fight(pc, v, { goo }, rng);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    CHECK(res.won, "start-of-round burn finishing the last enemy wins the fight");
    CHECK(res.kills == 1, "slain enemy is counted on the dot walk-off");
    CHECK(!res.fled, "no corpse-round flee throws the victory away");
    CHECK(out.str().find("slip away") == std::string::npos, "no flee was attempted");
}

void testFleeCountsSlainEnemies() {
    using namespace combat;
    core::Rng rng(9999);
    Character pc(ClassId::Warrior);
    Vault v;

    Enemy squire;
    squire.name = "Squire Test";
    squire.hpMax = squire.hp = 1;
    squire.attack = 0;
    squire.xpReward = 0;
    squire.goldReward = 0;

    Enemy overlord;
    overlord.name = "Overlord Test";
    overlord.hpMax = overlord.hp = 9999;
    overlord.attack = 0;
    overlord.xpReward = 0;
    overlord.goldReward = 0;

    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream in("a\n1\nf\nf\nf\nf\nf\nf\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const Result res = fight(pc, v, { squire, overlord }, rng);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    CHECK(res.fled, "the party slips away from the overlord");
    CHECK(res.kills == 1, "the already-slain squire is tallied on flee");
    CHECK(v.bestiary.enemyKills("Squire Test") == 1, "bestiary records the squire");
    CHECK(v.bestiary.enemyKills("Overlord Test") == 0, "the living overlord is not bookkept");
}

void testEnemyCritApplied() {
    using namespace combat;
    const std::uint64_t seed = 1234;
    const auto firstHit = [](const std::string& text) {
        const std::string mark = "hits you for ";
        const auto p = text.find(mark);
        if (p == std::string::npos) return 0;
        return std::atoi(text.c_str() + p + mark.size());
    };

    core::Rng rngA(seed);
    Character a(ClassId::Mage);
    Vault va;
    Enemy sane;
    sane.name = "Blade";
    sane.hpMax = sane.hp = 9999;
    sane.attack = 6;
    sane.defense = 0;
    sane.critChance = 0;
    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream inA("a\nf\nf\nf\nf\nf\nf\n");
    std::streambuf* oldIn = std::cin.rdbuf(inA.rdbuf());
    combat::fight(a, va, { sane }, rngA);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    core::Rng rngB(seed);
    Character b(ClassId::Mage);
    Vault vb;
    Enemy savage = sane;
    savage.critChance = 100;
    std::ostringstream out2;
    std::streambuf* oldOut2 = std::cout.rdbuf(out2.rdbuf());
    std::istringstream inB("a\nf\nf\nf\nf\nf\nf\n");
    std::streambuf* oldIn2 = std::cin.rdbuf(inB.rdbuf());
    combat::fight(b, vb, { savage }, rngB);
    std::cin.rdbuf(oldIn2);
    std::cin.clear();
    std::cout.rdbuf(oldOut2);

    const int calmHit = firstHit(out.str());
    const int critHit = firstHit(out2.str());
    CHECK(calmHit >= 4 && calmHit <= 7, "calm enemy round-1 hit is in the expected band");
    CHECK(critHit == calmHit * 3 / 2 || critHit > calmHit + 1,
          "a guaranteed-crit enemy lands a visibly larger first hit");
    CHECK(out2.str().find("(CRIT)") != std::string::npos, "the crit is announced");
}

void testBossNameOrder() {
    using namespace combat;
    core::Rng r1(50);
    CHECK(std::string(makeBoss(5, r1, 0).name) == "Twin Fang",
          "floor 5 spawns the first listed boss");
    core::Rng r2(51);
    CHECK(std::string(makeBoss(10, r2, 0).name) == "Baron Gore",
          "floor 10 spawns the second listed boss");
    core::Rng r3(52);
    CHECK(std::string(makeBoss(50, r3, 0).name) == "Eternal One",
          "floor 50 spawns the last listed boss");
    core::Rng r4(53);
    CHECK(std::string(makeBoss(55, r4, 0).name) == "Twin Fang",
          "the lineup wraps back around at floor 55");
}

void testSaveWriteAtomic() {
    core::Rng rng(3);
    Character pc(ClassId::Rogue);
    Vault v;
    v.gold = 777;
    const std::string path = "test_atomic.rpg";
    CHECK(save::write(path, pc, v), "atomic save write succeeds");
    CHECK(std::filesystem::exists(path), "save file exists after write");
    CHECK(!std::filesystem::exists(path + ".tmp"), "no temp file is left behind");
    Character pc2(ClassId::Rogue);
    Vault v2;
    CHECK(save::read(path, &pc2, &v2), "atomic-written save reads back");
    CHECK(v2.gold == 777, "gold roundtrips through the atomic write");
    CHECK(!save::write("no_such_dir/foo.rpg", pc, v), "write into a missing dir fails cleanly");
    std::filesystem::remove(path);
}

void testHeirloomExplorationGold() {
    ui::setForcePlain(true);
    const int floor = 5;

    std::uint64_t seed = 0;
    int plainGain = 0;
    for (std::uint64_t s = 1; s <= 2000 && seed == 0; ++s) {
        core::Rng rng(s);
        Character pc(ClassId::Warrior);
        Vault v;
        const int before = v.gold;
        std::ostringstream out;
        std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
        ui::floorEvent(pc, v, floor, rng);
        std::cout.rdbuf(oldOut);
        if (v.gold > before) {
            seed = s;
            plainGain = v.gold - before;
        }
    }

    core::Rng boostedRng(seed);
    Character boostedPc(ClassId::Warrior);
    Vault boosted;
    boosted.perks[static_cast<std::size_t>(PerkId::Heirloom)] = true;
    const int beforeBoosted = boosted.gold;
    std::ostringstream out2;
    std::streambuf* oldOut2 = std::cout.rdbuf(out2.rdbuf());
    ui::floorEvent(boostedPc, boosted, floor, boostedRng);
    std::cout.rdbuf(oldOut2);
    const int boostedGain = boosted.gold - beforeBoosted;

    ui::setForcePlain(false);

    CHECK(seed != 0, "some seed lands on a gold-granting floor event");
    CHECK(boostedGain == plainGain + plainGain * 20 / 100,
          "Heirloom boosts exploration gold by 20%");
    CHECK(boosted.totalGoldEarned == boostedGain,
          "totalGoldEarned tracks the boosted gold");
}

void testTrainingStats() {
    Character pc(ClassId::Warrior);
    Vault v;
    CHECK(pc.skillPoints() >= 1, "warrior opens with skill points");
    pc.gainXp(100'000, 0);
    Character plain(ClassId::Warrior);
    plain.gainXp(100'000, 0);
    CHECK(pc.stats(v).attack == plain.stats(v).attack, "baseline stats match at the same level");

    CHECK(pc.train(TrainId::Might), "train Might");
    CHECK(pc.trainRank(TrainId::Might) == 1, "rank increments");
    CHECK(pc.stats(v).attack == plain.stats(v).attack + 3, "Might adds 3 ATK per rank");
    CHECK(pc.train(TrainId::Vitality), "train Vitality");
    CHECK(pc.stats(v).maxHp == plain.stats(v).maxHp + 10, "Vitality adds 10 max HP per rank");
    CHECK(pc.train(TrainId::Focus), "train Focus");
    CHECK(pc.stats(v).maxResource == plain.stats(v).maxResource + 3, "Focus adds 3 max resource per rank");
    CHECK(pc.train(TrainId::Tenacity), "train Tenacity");
    CHECK(pc.stats(v).defense == plain.stats(v).defense + 1, "Tenacity adds 1 defense per rank");
    CHECK(pc.train(TrainId::Fleetness), "train Fleetness");
    CHECK(pc.stats(v).regenPerTurn == plain.stats(v).regenPerTurn + 1, "Fleetness adds 1 regen per rank");

    while (pc.train(TrainId::Might)) {}
    CHECK(pc.trainRank(TrainId::Might) == kTrainMaxRank, "Might caps at max rank");
    CHECK(!pc.train(TrainId::Might), "cannot train past the cap");
    CHECK(pc.stats(v).attack == plain.stats(v).attack + 3 * kTrainMaxRank,
          "capped Might feeds stats");
}

void testAddMergesPotions() {
    Vault v;
    v.add(makeVendorPotion(1, false));
    v.add(makeVendorPotion(1, false));
    CHECK(v.items.size() == 1, "add() merges same-kind potions");
    CHECK(v.items[0].count == 2, "add() potion counts accumulate");
    v.add(makeVendorPotion(2, true));
    CHECK(v.items.size() == 2, "add() keeps different kinds separate");
    CHECK(v.items[1].manaRestore > 0 && v.items[1].heal == 0, "second stack is the mana draught");
}

void testPotionStack() {
    Vault v;
    Item h1 = makeVendorPotion(1, false);
    h1.count = 2;
    v.addPotion(h1);
    v.addPotion(h1);
    CHECK(v.items.size() == 1, "same-kind potions merge into one stack");
    CHECK(v.items[0].count == 4, "stack count accumulates");
    CHECK(v.items[0].stacks(), "potion kind stackable");
    CHECK(potionHeal(v.items[0], 1) == potionAmount(PotionKind::Healing, 1), "floor-scaled heal");
    CHECK(potionHeal(v.items[0], 10) == potionAmount(PotionKind::Healing, 10), "heal scales with floor");
    const Item m = makeVendorPotion(1, true);
    v.addPotion(m);
    CHECK(v.items.size() == 2, "different kinds stay separate");
    CHECK(potionHeal(v.items[1], 5) == 0 && potionMana(v.items[1], 5) > 0, "mana draught heals none");
    v.bindBelt(0, PotionKind::Healing);
    CHECK(v.beltKind(0) == PotionKind::Healing, "belt bound to healing kind");
    CHECK(v.beltIndex(0) == 0, "belt resolves to the healing stack");
    for (int i = 0; i < 4; ++i) CHECK(v.usePotion(0), "drain a stack charge each use");
    CHECK(v.items.size() == 1, "emptied stack is erased");
    CHECK(v.beltIndex(0) < 0, "belt slot clears when no stack of that kind remains");
}

void testBeltKind() {
    Vault v;
    CHECK(v.beltKind(0) == PotionKind::None && v.beltKind(1) == PotionKind::None, "belts start empty");
    v.bindBelt(1, PotionKind::Mana);
    CHECK(v.beltKind(1) == PotionKind::Mana, "slot 2 binds mana");
    CHECK(v.beltKind(0) == PotionKind::None, "slot 1 untouched");
    v.bindBelt(1, PotionKind::Mana);
    v.bindBelt(0, PotionKind::None);
    CHECK(v.beltKind(0) == PotionKind::None, "explicit unbind clears the slot");
    CHECK(v.beltKind(1) == PotionKind::Mana, "unbinding one slot spares the other");
}

void testVersion9Roundtrip() {
    core::Rng rng(4);
    const std::string path = "test_v9.rpg";
    Character pc(ClassId::Mage);
    pc.setFloor(15);
    pc.setHardcore(true);
    pc.gainXp(500, 0);
    CHECK(pc.spendPoint(0, 0), "open a spell");
    CHECK(pc.train(TrainId::Focus), "train Focus once");
    CHECK(pc.train(TrainId::Focus), "train Focus twice");

    Vault v;
    v.gold = 55;
    v.items.push_back(makeGear(8, rng));
    v.equip(0);
    Item h = makeVendorPotion(15, false);
    h.count = 3;
    v.addPotion(h);
    Item m = makeVendorPotion(15, true);
    m.count = 2;
    v.addPotion(m);
    v.bindBelt(0, PotionKind::Healing);
    v.bindBelt(1, PotionKind::Mana);
    CHECK(save::write(path, pc, v, &rng), "write save");

    // Downgrade the v10 body to the v9 shape: drop the rng line, trim the
    // equipped list back to seven slots, bump the header, and re-sign.
    {
        std::ifstream in(path);
        std::string all((std::istreambuf_iterator<char>(in)), {});
        const auto sigPos = all.rfind("[sig]\n");
        CHECK(sigPos != std::string::npos, "save carries a signature block");
        std::string body = all.substr(0, sigPos);
        const auto rp = body.find("rng=");
        if (rp != std::string::npos) {
            const auto rngEnd = body.find('\n', rp);
            body.erase(rp, rngEnd - rp + 1);
        }
        const auto ep = body.find("equipped=");
        const auto eqEnd = body.find('\n', ep);
        std::istringstream ls(body.substr(ep + 9, eqEnd - (ep + 9)));
        std::vector<int> nums;
        int x = 0;
        while (ls >> x) nums.push_back(x);
        std::string eq = "equipped=";
        for (int i = 0; i < 7 && i < static_cast<int>(nums.size()); ++i) {
            if (i) eq += " ";
            eq += std::to_string(nums[static_cast<std::size_t>(i)]);
        }
        body.replace(ep, eqEnd - ep, eq);
        body.replace(0, body.find('\n'), "RPGSAVE v9");
        unsigned char sum = 0;
        for (const char ch : body) sum ^= static_cast<unsigned char>(ch);
        std::ofstream out(path, std::ios::trunc);
        out << body << "[sig]\n" << static_cast<int>(sum) << "\n";
    }

    Character pc2(ClassId::Warrior);
    Vault v2;
    CHECK(save::read(path, &pc2, &v2), "v9 save still loads");
    CHECK(pc2.classId() == ClassId::Mage, "v9 class survives the legacy load");
    CHECK(pc2.hardcore(), "v9 hardcore flag survives");
    CHECK(v2.equippedIndex(Slot::Trinket) == -1, "absent trinket field defaults to empty");
    CHECK(v2.belt == v.belt, "v9 belt kinds roundtrip");

    // a future/unknown version is still rejected
    {
        std::ifstream in(path);
        std::string all((std::istreambuf_iterator<char>(in)), {});
        const auto sigPos = all.rfind("[sig]\n");
        std::string body = all.substr(0, sigPos);
        body.replace(0, body.find('\n'), "RPGSAVE v11");
        unsigned char sum = 0;
        for (const char ch : body) sum ^= static_cast<unsigned char>(ch);
        std::ofstream out(path, std::ios::trunc);
        out << body << "[sig]\n" << static_cast<int>(sum) << "\n";
    }
    Character pc3(ClassId::Warrior);
    Vault v3;
    CHECK(!save::read(path, &pc3, &v3), "v11 header rejected by v10 reader");
    std::remove(path.c_str());
}

void testVersion10Roundtrip() {
    const std::string path = "test_v10.rpg";
    core::Rng rng(20240607);
    for (int i = 0; i < 5; ++i) (void)rng.next();

    Character pc(ClassId::Paladin);
    pc.setFloor(30);
    Vault v;
    const Item t = makeTrinket(30, rng);
    CHECK(t.slot == Slot::Trinket && t.relic != RelicPower::None,
          "makeTrinket yields a powered trinket");
    v.add(t);
    v.equip(static_cast<int>(v.items.size()) - 1);
    CHECK(save::write(path, pc, v, &rng), "write v10 save with rng state");

    core::Rng resumed(1);
    Character pc2(ClassId::Warrior);
    Vault v2;
    CHECK(save::read(path, &pc2, &v2, &resumed), "read v10 save");
    CHECK(pc2.classId() == ClassId::Paladin, "Paladin class int roundtrips");
    CHECK(v2.equippedIndex(Slot::Trinket) == 0, "eighth trinket slot roundtrips");
    CHECK(v2.items.size() == 1 && v2.items[0].relic == t.relic, "relic power roundtrips");
    CHECK(resumed.next() == rng.next(), "rng stream resumes exactly");
    CHECK(resumed.next() == rng.next(), "rng stream stays in sync");
    std::remove(path.c_str());
}

void testGloryTrack() {
    const std::string path = "glory.rpg";
    std::remove(path.c_str());

    Character pc(ClassId::Warrior);
    Vault v;
    CHECK(glory::unlockedCount(glory::load()) == 0, "fresh glory is empty");
    glory::sync(pc, v);
    CHECK(glory::unlockedCount(glory::load()) == 0, "empty character earns nothing");

    v.kills = 120;
    v.bestFloor = 100;
    v.bossesSlain = 3;
    v.legendaryFound = 2;
    v.deaths = 1;
    v.mastery = 3;
    v.aspect = 1;
    v.totalGoldEarned = 12000;
    glory::sync(pc, v);
    const auto d1 = glory::load();
    CHECK(glory::unlockedCount(d1) == 11, "all softcore + stat achievements earned");
    CHECK(d1.unlocked[0] && !d1.unlocked[11], "First Blood earned, Hardcore Heart not");
    CHECK(!d1.unlocked[12] && !d1.unlocked[13], "hardcore-floor achievements gated on hardcore");
    CHECK(!d1.unlocked[14], "Pay the Iron Price needs a permadeath");

    glory::fall(pc, v);
    const auto d2 = glory::load();
    CHECK(glory::unlockedCount(d2) == 12, "permadeath grants Pay the Iron Price");
    CHECK(d2.fallen.size() == 1, "one fallen hero recorded");
    CHECK(d2.fallen[0].cls == ClassId::Warrior && d2.fallen[0].kills == 120,
          "fallen entry holds class and kills");

    // persisted across a reload (account-wide)
    const auto d3 = glory::load();
    CHECK(d3.unlocked == d2.unlocked, "glory unlocks persist across loads");
    CHECK(d3.fallen.size() == 1, "fallen list persists across loads");

    // achievements stay earned for a brand-new character (derived-state memory)
    Character fresh(ClassId::Rogue);
    Vault v2;
    glory::sync(fresh, v2);
    const auto d4 = glory::load();
    CHECK(d4.unlocked[0], "account-wide achievements survive a new character");

    for (int i = 0; i < 60; ++i) glory::fall(pc, v);
    CHECK(glory::load().fallen.size() == 50, "fallen list is capped at 50");

    std::remove(path.c_str());
}

void testHardcorePermadeath() {
    const std::string savePath = "test_hc.rpg";
    const std::string gloryPath = "glory.rpg";
    std::remove(savePath.c_str());
    std::remove(gloryPath.c_str());

    core::Rng rng(50);
    Character pc(ClassId::Warrior);
    pc.setHardcore(true);
    pc.setFloor(40);
    Vault v;
    v.gold = 50;
    CHECK(save::write(savePath, pc, v), "hardcore save seeded");
    CHECK(save::peek(savePath).hardcore, "peek exposes the hardcore flag");

    std::string feed = "a\na\na\na\na\na\na\na\na\na\n";
    std::istringstream in(feed);
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    game::adventure(pc, v, rng, savePath, {});
    std::cin.rdbuf(oldIn);
    std::cin.clear();

    CHECK(pc.hp() == 0, "hardcore hero died in the Rift");
    CHECK(!save::read(savePath, &pc, &v), "hardcore death erases the save file");
    const auto d = glory::load();
    CHECK(d.fallen.size() == 1, "hardcore death leaves a grave in the glory track");
    CHECK(d.unlocked[14], "Pay the Iron Price unlocked by permadeath");
    CHECK(d.unlocked[11], "Hardcore Heart remembered from the doomed run");

    std::remove(savePath.c_str());
    std::remove(gloryPath.c_str());
}

void testAmbienceSweep() {
    using namespace combat;
    std::vector<std::string> seen;
    for (int f = 1; f <= 72; ++f) {
        const std::string b = biomeFor(f);
        const std::string l = biomeLoreFor(f);
        CHECK(!l.empty(), "every biome has lore");
        if (seen.empty() || seen.back() != b) seen.push_back(b);
    }
    CHECK(seen.size() == 12, "twelve biome zones across the sweep");
    CHECK(std::string(biomeLoreFor(1)) != std::string(biomeLoreFor(6)), "lore varies per biome");
}

void testUiLayoutPlain() {
    ui::setForcePlain(true);

    const int W = ui::layoutWidth();
    const std::string lorem =
        "Cool waters mend your wounds and restore broken will; the shrine answers "
        "with a runestone that forms slowly in your palm, warm as a candle flame.";

    const std::string wr = ui::wrap(lorem, 20);
    CHECK(wr.find('\n') != std::string::npos, "long text wraps at narrow width");
    std::size_t pos = 0;
    bool bounded = true;
    while (pos <= wr.size()) {
        const std::size_t nl = wr.find('\n', pos);
        std::string seg = wr.substr(pos, nl == std::string::npos
                                    ? std::string::npos : nl - pos);
        if (seg.size() > 20) bounded = false;
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    CHECK(bounded, "no wrapped line exceeds its budget");

    std::vector<std::string> srcWords, wrWords;
    {
        std::istringstream ls(lorem);
        std::string t;
        while (ls >> t) srcWords.push_back(t);
        std::istringstream lw(wr);
        while (lw >> t) wrWords.push_back(t);
    }
    CHECK(srcWords == wrWords, "wrap preserves word order");

    CHECK(ui::wrap("short", 20) == "short", "short text untouched by wrap");
    const std::string hard = ui::wrap("supercalifragilisticexpialidocious", 8);
    CHECK(hard.find('\n') != std::string::npos, "oversized words are hard-split");
    std::size_t p = 0, hardWidth = 0;
    while (p <= hard.size()) {
        const std::size_t nl = hard.find('\n', p);
        std::string seg = hard.substr(p, nl == std::string::npos
                                      ? std::string::npos : nl - p);
        hardWidth = std::max(hardWidth, seg.size());
        if (nl == std::string::npos) break;
        p = nl + 1;
    }
    CHECK(hardWidth <= 8, "hard-split keeps every segment within budget");

    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    ui::panelTop("A Puzzling Obelisk", ui::c::accent);
    ui::panelLine("  Two runes glow - one grants fortune, one brings misfortune.");
    {
        const std::string head = "   " + std::string(ui::gly(ui::G_MED)) + " name";
        const int headLen = 3 + 1 + 1 + 4;
        const int budget = std::max(W - 2 - headLen, 4);
        const std::string wrapped = ui::wrap(lorem, budget);
        const std::string pad(std::size_t(headLen), ' ');
        std::size_t q = 0;
        while (true) {
            const std::size_t nl = wrapped.find('\n', q);
            ui::panelLine((q == 0 ? head : pad)
                          + wrapped.substr(q, nl == std::string::npos
                          ? std::string::npos : nl - q));
            if (nl == std::string::npos) break;
            q = nl + 1;
        }
    }
    ui::panelLine(ui::chip(5) + std::string(ui::gly(ui::G_POTION))
                  + " Belt  bind quick potions (1/2 in combat)");
    ui::panelBottom(ui::c::accent);
    std::cout.rdbuf(oldOut);

    const std::string all = out.str();
    std::size_t q2 = 0;
    bool anyOverflow = false, anyHighByte = false;
    while (q2 < all.size()) {
        const std::size_t nl = all.find('\n', q2);
        const std::string line = all.substr(q2, nl == std::string::npos
                                            ? std::string::npos : nl - q2);
        if (!line.empty() && line.size() > static_cast<std::size_t>(W)) anyOverflow = true;
        for (unsigned char ch : line)
            if (ch >= 0x80) anyHighByte = true;
        if (nl == std::string::npos) break;
        q2 = nl + 1;
    }
    CHECK(!anyOverflow, "panel output stays within the clamped layout width");
    CHECK(all.find("\x1b[") == std::string::npos, "plain mode embeds no ANSI escapes");
    CHECK(!anyHighByte, "plain-mode panel output is 7-bit ASCII");

    ui::setForcePlain(false);
}

void testQuitSaveFlush() {
    // 1.1 regression: quitting the camp persists the floor just cleared.
    // game::adventure covers its own writes when no callback is supplied.
    core::Rng rng(99);
    Character pc(ClassId::Warrior);
    Vault v;
    v.clear();
    v.gold = 50;
    const std::string path = "test_quit_save.rpg";
    std::remove(path.c_str());

    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream in("a\n0\n");   // basic-attack the fight, then quit camp
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    game::adventure(pc, v, rng, path, {});
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    CHECK(v.gold > 50, "floor rewards kept across camp quit");
    Character pc2(ClassId::Warrior);
    Vault v2;
    CHECK(save::read(path, &pc2, &v2) && v2.kills > 0, "quit-save flushed fight record to disk");
    CHECK(v.kills == v2.kills, "in-memory and flushed kill counts agree");
    std::remove(path.c_str());
}

void testAdrenalineKeptOnPhase() {
    using namespace combat;
    Character pc(ClassId::Warrior);
    Vault v;
    Item sword;
    sword.slot = Slot::Weapon;
    sword.power = 50;
    v.add(sword);
    v.equip(0);
    const EffectiveStats st = pc.stats(v);
    const Spell& fire = classSpells(ClassId::Mage)[0];

    std::ostringstream sink;
    std::streambuf* oldOut = std::cout.rdbuf(sink.rdbuf());

    int phasedSeed = -1, landedSeed = -1, spellPhaseSeed = -1;
    for (std::uint64_t seed = 1;
         seed <= 400 && (phasedSeed < 0 || landedSeed < 0 || spellPhaseSeed < 0); ++seed) {
        core::Rng rng(seed);
        Enemy e;
        e.name = "Wraith";
        e.hpMax = e.hp = 1000;
        e.affixes.push_back(EnemyAffix::Ethereal);
        int adren = 1;
        const int dmg = strikeOnce(pc, st, e, rng, adren, 0);
        if (dmg == 0 && phasedSeed < 0) phasedSeed = static_cast<int>(seed);
        if (dmg > 0 && landedSeed < 0) landedSeed = static_cast<int>(seed);

        core::Rng rng2(seed);
        Enemy e2;
        e2.name = "Wraith";
        e2.hpMax = e2.hp = 1000;
        e2.affixes.push_back(EnemyAffix::Ethereal);
        int adren2 = 1;
        if (castOnce(pc, st, fire, e2, rng2, adren2, 0) == 0 && spellPhaseSeed < 0)
            spellPhaseSeed = static_cast<int>(seed);
    }

    core::Rng ph(static_cast<std::uint64_t>(phasedSeed));
    Enemy w;
    w.name = "Wraith";
    w.hpMax = w.hp = 1000;
    w.affixes.push_back(EnemyAffix::Ethereal);
    int adrenPh = 1;
    const int dmgPh = strikeOnce(pc, st, w, ph, adrenPh, 0);

    core::Rng ld(static_cast<std::uint64_t>(landedSeed));
    Enemy w2;
    w2.name = "Wraith";
    w2.hpMax = w2.hp = 1000;
    w2.affixes.push_back(EnemyAffix::Ethereal);
    int adrenLd = 1;
    const int dmgLd = strikeOnce(pc, st, w2, ld, adrenLd, 0);

    core::Rng r0(77), r1(77);
    Enemy plain0, plain1;
    plain0.name = plain1.name = "Dummy";
    plain0.hpMax = plain0.hp = plain1.hpMax = plain1.hp = 1000;
    int adren0 = 0, adren1 = 1;
    const int dmg0 = strikeOnce(pc, st, plain0, r0, adren0, 0);
    const int dmg1 = strikeOnce(pc, st, plain1, r1, adren1, 0);

    std::cout.rdbuf(oldOut);

    CHECK(dmgPh == 0 && adrenPh == 1, "a phased attack keeps the adrenaline stack");
    CHECK(dmgLd > 0 && adrenLd == 0, "a landed attack spends the adrenaline stack");
    CHECK(dmg1 > dmg0, "an adrenaline stack boosts the next hit by 15%");
    CHECK(spellPhaseSeed >= 0, "spells can also phase against Ethereal foes");
}

void testSpellBlurbScaling() {
    const Spell& s = classSpells(ClassId::Mage)[0];
    const int raw = spellDamage(s, 5, 1);
    CHECK(spellBlurb(s, 5, 1, 0).find(std::to_string(raw) + " dmg") != std::string::npos,
          "spell blurb shows raw damage at 0 ATK");
    CHECK(spellBlurb(s, 5, 1, 100).find(std::to_string(raw * 2) + " dmg") != std::string::npos,
          "spell blurb applies the caster's ATK bonus");
    CHECK(spellBlurb(s, 5, 5, 0).find(std::to_string(spellDamage(s, 5, 5)) + " dmg")
              != std::string::npos,
          "spell blurb damage scales with floor");
}

void testBeltDedupe() {
    using namespace combat;
    core::Rng rng(99);
    Character pc(ClassId::Warrior);
    Vault v;
    Item h = makeVendorPotion(1, false);
    h.count = 3;
    v.addPotion(h);
    v.bindBelt(0, PotionKind::Healing);
    v.bindBelt(1, PotionKind::Healing);

    Enemy weak;
    weak.name = "Pebble";
    weak.hpMax = weak.hp = 3;
    weak.attack = 1;
    weak.xpReward = 1;
    weak.goldReward = 1;

    std::string input = "i\n0\n";
    for (int i = 0; i < 20; ++i) input += "a\n";
    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::istringstream in(input);
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    (void)fight(pc, v, { weak }, rng);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    std::cout.rdbuf(oldOut);

    std::size_t belts = 0, pos = 0;
    while ((pos = out.str().find("(belt)", pos)) != std::string::npos) {
        ++belts;
        pos += 6;
    }
    CHECK(belts == 1, "one potion stack bound to both belt slots lists once");
}

void testTrapDodge() {
    ui::setForcePlain(true);
    std::istringstream noInput("");
    std::streambuf* oldIn = std::cin.rdbuf(noInput.rdbuf());
    bool sawDodge = false, sawHit = false;
    for (std::uint64_t seed = 1; seed <= 400 && !(sawDodge && sawHit); ++seed) {
        core::Rng rng(seed);
        Character pc(ClassId::Warrior);
        Vault v;
        const int before = pc.hp();
        std::ostringstream out;
        std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
        ui::floorEvent(pc, v, 1, rng);
        std::cout.rdbuf(oldOut);
        if (out.str().find("A Hidden Trap!") == std::string::npos) continue;
        const int d = std::max(1, pc.stats(v).maxHp / 4);
        if (pc.hp() == before) sawDodge = true;
        else if (pc.hp() == before - d) sawHit = true;
    }
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    ui::setForcePlain(false);
    CHECK(sawDodge, "a hidden trap can be dodged");
    CHECK(sawHit, "a hidden trap can land for a quarter max HP");
}

void testDisplayWidth() {
    CHECK(ui::displayWidth("abc") == 3, "ASCII text width counts columns");
    CHECK(ui::displayWidth("\u2014") == 1, "an em dash is one column, not three bytes");
    CHECK(ui::displayWidth("A\u2014B") == 3, "mixed ASCII and UTF-8 width");
    const std::string centered = ui::center("\u2014", 5);
    CHECK(ui::displayWidth(centered) == 5, "center pads by display width, not byte count");
}

void testAskIntTrim() {
    std::istringstream in("2  \r\n");
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    const int v = rpg::io::askInt("pick", 1, 3);
    std::cin.rdbuf(oldIn);
    std::cin.clear();
    CHECK(v == 2, "askInt tolerates trailing spaces and carriage returns");
}

void testMasteryCap() {
    Character pc(ClassId::Warrior);
    Vault huge;
    huge.mastery = 100000;
    Vault capped;
    capped.mastery = 150;
    const auto a = pc.stats(huge);
    const auto b = pc.stats(capped);
    CHECK(a.attack == b.attack, "mastery attack exponent caps at a sane bound");
    CHECK(a.maxHp == b.maxHp, "mastery HP exponent caps at a sane bound");
    CHECK(a.maxResource == b.maxResource, "mastery resource exponent caps at a sane bound");
}

void testRadiantReflect() {
    using namespace combat;
    Character pc(ClassId::Warrior);
    Vault v;
    Item sword;
    sword.slot = Slot::Weapon;
    sword.power = 40;
    v.add(sword);
    v.equip(0);
    const EffectiveStats st = pc.stats(v);
    const int full = pc.hp();

    Enemy e;
    e.name = "Glow";
    e.hpMax = e.hp = 1000;
    e.affixes.push_back(EnemyAffix::Radiant);

    core::Rng rng(5);
    int adren = 0;
    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    const int dealt = strikeOnce(pc, st, e, rng, adren, 0);
    std::cout.rdbuf(oldOut);

    CHECK(dealt > 0, "radiant enemy takes a hit");
    CHECK(pc.hp() == full - dealt * 20 / 100, "Radiant reflects 20% of damage dealt");
}

void testSummonerAffix() {
    using namespace combat;
    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());

    Enemy summoner;
    summoner.name = "Conjurer";
    summoner.hpMax = summoner.hp = 100;
    summoner.attack = 10;
    summoner.defense = 4;
    summoner.affixes.push_back(EnemyAffix::Summoner);
    std::vector<Enemy> es{ summoner };

    CHECK(!resolveSummon(es[0], es), "summoner stays quiet above half HP");
    es[0].hp = 50;
    CHECK(resolveSummon(es[0], es), "summoner calls a thrall at half HP");
    CHECK(es.size() == 2 && es[1].name == "Dark Thrall", "a Dark Thrall joins the fight");
    CHECK(es[1].hpMax == 25, "thrall HP is a quarter of the summoner's");
    CHECK(es[1].attack == 5, "thrall attack is half the summoner's");
    CHECK(!resolveSummon(es[0], es), "summoner only calls one thrall");

    Enemy boss;
    boss.name = "Boss";
    boss.boss = true;
    boss.hpMax = boss.hp = 80;
    boss.attack = 12;
    std::vector<Enemy> bs{ boss };
    CHECK(!resolveSummon(bs[0], bs), "boss holds its summon above half HP");
    bs[0].hp = 40;
    CHECK(resolveSummon(bs[0], bs), "boss still summons at half HP");
    CHECK(!resolveSummon(bs[0], bs), "boss summon is once-only");

    std::cout.rdbuf(oldOut);
    CHECK(out.str().find("summons a Dark Thrall") != std::string::npos, "summon is announced");
}

void testHexerDebuff() {
    using namespace combat;
    bool sawHex = false, sawDotTick = false;
    for (std::uint64_t seed = 1; seed <= 200 && !(sawHex && sawDotTick); ++seed) {
        Character pc(ClassId::Warrior);
        Vault v;
        Item sword;
        sword.slot = Slot::Weapon;
        sword.power = 50;
        v.add(sword);
        v.equip(0);

        Enemy hexer;
        hexer.name = "Hexer";
        hexer.hpMax = hexer.hp = 300;
        hexer.attack = 1;
        hexer.affixes.push_back(EnemyAffix::Hexer);

        std::string input;
        for (int i = 0; i < 12; ++i) input += "a\n";
        core::Rng rng(seed);
        std::ostringstream out;
        std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
        std::istringstream in(input);
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        (void)fight(pc, v, { hexer }, rng);
        std::cin.rdbuf(oldIn);
        std::cin.clear();
        std::cout.rdbuf(oldOut);

        const std::string text = out.str();
        if (text.find("hexes you:") != std::string::npos) sawHex = true;
        if (text.find("bites you for") != std::string::npos) sawDotTick = true;
    }
    CHECK(sawHex, "hexer curses the player on a hit");
    CHECK(sawDotTick, "a hexer's damage-over-time bites the player");
}

void testNewFloorEvents() {
    ui::setForcePlain(true);

    auto runEvent = [](std::uint64_t seed, int floor, const std::string& input,
                       Character& pc, Vault& v) -> std::string {
        core::Rng rng(seed);
        std::ostringstream out;
        std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
        std::istringstream in(input);
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        ui::floorEvent(pc, v, floor, rng);
        std::cin.rdbuf(oldIn);
        std::cin.clear();
        std::cout.rdbuf(oldOut);
        return out.str();
    };
    auto findSeed = [&](const char* title) -> std::uint64_t {
        for (std::uint64_t seed = 1; seed <= 2000; ++seed) {
            Character pc(ClassId::Warrior);
            Vault v;
            if (runEvent(seed, 5, "", pc, v).find(title) != std::string::npos) return seed;
        }
        return 0;
    };

    const std::uint64_t altarSeed = findSeed("A Blessing Altar");
    CHECK(altarSeed != 0, "blessing altar appears from some seed");
    if (altarSeed != 0) {
        Character healPc(ClassId::Warrior);
        Vault hv;
        const int maxHp = healPc.stats(hv).maxHp;
        healPc.takeDamage(healPc.hp() - 1);
        (void)runEvent(altarSeed, 5, "1\n", healPc, hv);
        CHECK(healPc.hp() == std::min(maxHp, 1 + maxHp / 2), "blessing altar heals half max HP");

        Character runePc(ClassId::Warrior);
        Vault rv;
        (void)runEvent(altarSeed, 5, "2\n", runePc, rv);
        CHECK(rv.runes.size() == 1, "blessing altar's cold rune grants a runestone");
    }

    const std::uint64_t demonSeed = findSeed("A Wandering Demon");
    CHECK(demonSeed != 0, "wandering demon appears from some seed");
    if (demonSeed != 0) {
        Character pc(ClassId::Warrior);
        Vault v;
        v.gold = 100;
        (void)runEvent(demonSeed, 2, "1\n", pc, v);
        CHECK(v.gold >= 60 && v.gold <= 60 + 8 * 2, "demon wager spends 40g and may pay out");

        Character poorPc(ClassId::Warrior);
        Vault poorV;
        poorV.gold = 10;
        (void)runEvent(demonSeed, 2, "1\n", poorPc, poorV);
        CHECK(poorV.gold == 10, "demon leaves a broke player alone");
    }

    const std::uint64_t fireSeed = findSeed("An Abandoned Campfire");
    CHECK(fireSeed != 0, "abandoned campfire appears from some seed");
    if (fireSeed != 0) {
        Character healPc(ClassId::Warrior);
        Vault hv;
        const int maxHp = healPc.stats(hv).maxHp;
        healPc.takeDamage(healPc.hp() - 1);
        (void)runEvent(fireSeed, 4, "1\n", healPc, hv);
        CHECK(healPc.hp() == maxHp, "campfire rest restores full HP");

        Character xpPc(ClassId::Warrior);
        Vault xv;
        xv.runes.push_back(Rune{});
        const int xpBefore = xpPc.xp();
        (void)runEvent(fireSeed, 4, "2\n", xpPc, xv);
        CHECK(xv.runes.empty(), "campfire consumes a runestone");
        CHECK(xpPc.xp() > xpBefore || xpPc.level() > 1, "campfire converts a runestone into XP");

        Character dryPc(ClassId::Warrior);
        Vault dryV;
        (void)runEvent(fireSeed, 4, "2\n", dryPc, dryV);
        CHECK(dryV.runes.empty() && dryPc.xp() == 0, "campfire without a runestone does nothing");
    }

    const std::uint64_t echoSeed = findSeed("Echo of a Past Hero");
    CHECK(echoSeed != 0, "echo of a past hero appears from some seed");
    if (echoSeed != 0) {
        Character gearPc(ClassId::Warrior);
        Vault gearV;
        const std::size_t before = gearV.items.size();
        (void)runEvent(echoSeed, 6, "1\n", gearPc, gearV);
        CHECK(gearV.items.size() == before + 1, "echo grants a piece of gear");

        Character runePc(ClassId::Warrior);
        Vault runeV;
        (void)runEvent(echoSeed, 6, "3\n", runePc, runeV);
        CHECK(runeV.runes.size() == 2, "echo grants two runestones");
    }

    ui::setForcePlain(false);
}

void testPerkStats() {
    Character pc(ClassId::Warrior);
    Vault v;
    const int baseRegen = pc.stats(v).regenPerTurn;
    v.perks[static_cast<std::size_t>(PerkId::Greed)]        = true;
    v.perks[static_cast<std::size_t>(PerkId::Regeneration)] = true;
    v.perks[static_cast<std::size_t>(PerkId::Evasion)]      = true;
    v.perks[static_cast<std::size_t>(PerkId::Bargain)]      = true;
    const EffectiveStats st = pc.stats(v);
    CHECK(st.lootBonusPct == 50, "Greed adds 50% to shard and essence loot");
    CHECK(st.regenPerTurn == baseRegen + 2, "Regeneration adds 2 HP regen per turn");
    CHECK(st.dodgeChance == 8, "Evasion adds 8% dodge");
    CHECK(st.vendorDiscountPct == 20, "Bargain takes 20% off vendor prices");
    CHECK(discountedCost(40, st.vendorDiscountPct) == 32, "Bargain discount applies to a price");
    CHECK(discountedCost(150, st.vendorDiscountPct) == 120, "Bargain scales with the price");
    CHECK(discountedCost(1, st.vendorDiscountPct) == 1, "Bargain never discounts below 1");
}

void testGreedLoot() {
    using namespace combat;
    bool grew = false;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        auto run = [&](bool greed) -> Loot {
            Character pc(ClassId::Warrior);
            Vault v;
            if (greed) v.perks[static_cast<std::size_t>(PerkId::Greed)] = true;
            Item sword;
            sword.slot = Slot::Weapon;
            sword.power = 500;
            v.add(sword);
            v.equip(0);
            Enemy boss;
            boss.name = "Pinata";
            boss.boss = true;
            boss.hpMax = boss.hp = 1;
            boss.xpReward = 1;
            boss.goldReward = 1;
            core::Rng rng(seed);
            std::ostringstream out;
            std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
            const Result r = fight(pc, v, { boss }, rng);
            std::cout.rdbuf(oldOut);
            return r.loot;
        };
        const Loot plain = run(false);
        const Loot greedy = run(true);
        CHECK(greedy.shards == plain.shards + plain.shards * 50 / 100,
              "Greed raises shard loot by half");
        CHECK(greedy.essence == plain.essence + plain.essence * 50 / 100,
              "Greed raises essence loot by half");
        if (greedy.shards > plain.shards) grew = true;
    }
    CHECK(grew, "Greed actually increases a loot roll");
}

void testEvasionPerk() {
    using namespace combat;
    bool sawEvade = false;
    for (std::uint64_t seed = 1; seed <= 80 && !sawEvade; ++seed) {
        Character pc(ClassId::Warrior);
        Vault v;
        v.perks[static_cast<std::size_t>(PerkId::Evasion)] = true;
        Enemy e;
        e.name = "Turtle";
        e.hpMax = e.hp = 100000;
        e.attack = 1;
        e.defense = 0;
        std::string input;
        for (int i = 0; i < 400; ++i) input += "a\n";
        core::Rng rng(seed);
        std::ostringstream out;
        std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
        std::istringstream in(input);
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        (void)fight(pc, v, { e }, rng);
        std::cin.rdbuf(oldIn);
        std::cin.clear();
        std::cout.rdbuf(oldOut);
        if (out.str().find("evade") != std::string::npos) sawEvade = true;
    }
    CHECK(sawEvade, "Evasion lets the player dodge an enemy attack");
}

void testPerkSaveRoundtrip() {
    const std::string path = "test_perks.rpg";
    Character pc(ClassId::Warrior);
    Vault v;
    v.perks[static_cast<std::size_t>(PerkId::Heirloom)]     = true;
    v.perks[static_cast<std::size_t>(PerkId::Greed)]        = true;
    v.perks[static_cast<std::size_t>(PerkId::Regeneration)] = true;
    v.perks[static_cast<std::size_t>(PerkId::Evasion)]      = true;
    v.perks[static_cast<std::size_t>(PerkId::Bargain)]      = true;
    CHECK(save::write(path, pc, v), "write perks save");
    Character pc2(ClassId::Warrior);
    Vault v2;
    CHECK(save::read(path, &pc2, &v2), "read perks save");
    CHECK(v2.perks == v.perks, "all Rift Aspect perks roundtrip through save");
    std::remove(path.c_str());
}

void testTrinketPowers() {
    Character pc(ClassId::Warrior);
    const auto with = [&](RelicPower r) {
        Vault v;
        Item t;
        t.slot = Slot::Trinket;
        t.relic = r;
        v.add(t);
        v.equip(static_cast<int>(v.items.size()) - 1);
        return pc.stats(v);
    };
    CHECK(with(RelicPower::Thorns).thornsPct == 20, "Thorns reflects 20%");
    CHECK(with(RelicPower::Potent).potionBonusPct == 25, "Potent boosts potions 25%");
    CHECK(with(RelicPower::GoldenTouch).goldGainPct == 20, "Golden Touch adds 20% gold");
    CHECK(with(RelicPower::Dodge).dodgeChance == 10, "Dodge adds 10% evasion");
    CHECK(with(RelicPower::Aegis).aegisGuard == 50, "Aegis grants Guard at fight start");
    CHECK(with(RelicPower::Scavenger).scavengePerKill == 1, "Scavenger grants a shard per kill");

    core::Rng rng(3);
    for (int i = 0; i < 40; ++i) {
        const Item t = makeTrinket(30, rng);
        CHECK(t.slot == Slot::Trinket, "makeTrinket always yields a trinket");
        CHECK(t.relic != RelicPower::None && t.relic != RelicPower::kNumRelics,
              "trinket power is a real relic");
    }
}

void testTrinketCombat() {
    using namespace combat;
    bool sawThorns = false;
    for (std::uint64_t seed = 1; seed <= 120 && !sawThorns; ++seed) {
        Character pc(ClassId::Warrior);
        Vault v;
        Item t;
        t.slot = Slot::Trinket;
        t.relic = RelicPower::Thorns;
        v.add(t);
        v.equip(static_cast<int>(v.items.size()) - 1);
        Enemy e;
        e.name = "Brute";
        e.hpMax = e.hp = 100000;
        e.attack = 5;
        e.defense = 0;
        std::string input;
        for (int i = 0; i < 200; ++i) input += "a\n";
        core::Rng rng(seed);
        std::ostringstream out;
        std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
        std::istringstream in(input);
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        (void)fight(pc, v, { e }, rng);
        std::cin.rdbuf(oldIn);
        std::cin.clear();
        std::cout.rdbuf(oldOut);
        if (out.str().find("thorns reflect") != std::string::npos) sawThorns = true;
    }
    CHECK(sawThorns, "Thorns reflects enemy damage back");

    bool sawBonus = false;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        const auto run = [&](bool scav) {
            Character pc(ClassId::Warrior);
            Vault v;
            Item sword;
            sword.slot = Slot::Weapon;
            sword.power = 500;
            v.add(sword);
            v.equip(0);
            if (scav) {
                Item t;
                t.slot = Slot::Trinket;
                t.relic = RelicPower::Scavenger;
                v.add(t);
                v.equip(static_cast<int>(v.items.size()) - 1);
            }
            Enemy e;
            e.name = "Mook";
            e.hpMax = e.hp = 1;
            e.xpReward = 1;
            e.goldReward = 1;
            core::Rng rng(seed);
            std::ostringstream out;
            std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
            const Result r = fight(pc, v, { e }, rng);
            std::cout.rdbuf(oldOut);
            return r.loot.shards;
        };
        const int plain = run(false);
        const int scav = run(true);
        CHECK(scav == plain + 1, "Scavenger adds a shard per kill");
        if (scav > plain) sawBonus = true;
    }
    CHECK(sawBonus, "Scavenger shards are actually awarded");
}

void testPaladinClass() {
    Character pal(ClassId::Paladin);
    Vault v;
    CHECK(classSpells(ClassId::Paladin).size() == 12, "Paladin has 12 spells");
    bool hasHeal = false, hasDebuff = false, hasShield = false;
    for (const auto& s : classSpells(ClassId::Paladin)) {
        if (s.type == SpellType::Heal) hasHeal = true;
        if (s.enemyAtkDownPct > 0 || s.enemyVulnPct > 0) hasDebuff = true;
        if (s.buffDefense > 0) hasShield = true;
    }
    CHECK(hasHeal && hasDebuff && hasShield, "Paladin tree covers damage, healing, and support");
    CHECK(std::string(resourceName(ClassId::Paladin)) == "Conviction",
          "Paladin resource is Conviction");
    CHECK(pal.stats(v).healAmpPct == 25, "Righteous Grace amplifies all healing by 25%");
    CHECK(Character(ClassId::Warrior).stats(v).healAmpPct == 0,
          "only the Paladin amplifies healing");

    using namespace combat;
    bool won = false;
    for (std::uint64_t seed = 1; seed <= 40 && !won; ++seed) {
        Character pc(ClassId::Paladin);
        Vault pv;
        Item sword;
        sword.slot = Slot::Weapon;
        sword.power = 500;
        pv.add(sword);
        pv.equip(0);
        Enemy e;
        e.name = "Training Dummy";
        e.hpMax = e.hp = 1;
        e.xpReward = 1;
        e.goldReward = 1;
        core::Rng rng(seed);
        std::ostringstream out;
        std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
        const Result r = fight(pc, pv, { e }, rng);
        std::cout.rdbuf(oldOut);
        if (r.won) won = true;
    }
    CHECK(won, "Paladin can win a basic fight");
}

// ---------------------------------------------------------------------------
// harness: RNG API
// ---------------------------------------------------------------------------

void testRngApi() {
    core::Rng a(9);
    for (int i = 0; i < 5000; ++i) {
        const double r = a.roll01();
        CHECK(r >= 0.0 && r < 1.0, "roll01 always in [0, 1)");
    }
    CHECK(a.pick(1) == 0, "pick(1) is forced to 0");
    core::Rng d(3);
    std::array<int, 4> seen{};
    int lo = 5, hi = -1;
    for (int i = 0; i < 2000; ++i) {
        const int v = static_cast<int>(d.pick(4));
        lo = std::min(lo, v);
        hi = std::max(hi, v);
        ++seen[static_cast<std::size_t>(v)];
    }
    CHECK(lo == 0 && hi == 3, "pick(4) stays within bounds");
    CHECK(seen[0] > 0 && seen[1] > 0 && seen[2] > 0 && seen[3] > 0, "pick(n) covers all buckets");
    {
        core::Rng c(5);
        bool never = true;
        for (int i = 0; i < 200; ++i)
            if (c.chance(0.0)) never = false;
        CHECK(never, "chance(0) is always false");
    }
    {
        core::Rng e(6);
        bool always = true;
        for (int i = 0; i < 200; ++i)
            if (!e.chance(1.0)) always = false;
        CHECK(always, "chance(1) is always true");
    }
    core::Rng p(77), q(77);
    for (int i = 0; i < 20; ++i) CHECK(p.next() == q.next(), "same seed streams in lockstep");
    const std::string state = p.save();
    std::array<std::uint64_t, 20> after{};
    for (int i = 0; i < 20; ++i) after[static_cast<std::size_t>(i)] = p.next();
    core::Rng r2(999);
    r2.load(state);
    for (int i = 0; i < 20; ++i)
        CHECK(r2.next() == after[static_cast<std::size_t>(i)], "load resumes the exact stream");
}

// ---------------------------------------------------------------------------
// harness: character edge cases
// ---------------------------------------------------------------------------

void testCharacterEdges() {
    Character pc(ClassId::Mage);
    Vault v;
    const int full = pc.hp();
    const int res = pc.resource();
    pc.takeDamage(-7);
    CHECK(pc.hp() == full, "negative damage is ignored");
    pc.takeDamage(0);
    CHECK(pc.hp() == full, "zero damage is ignored");
    pc.takeDamage(3);
    CHECK(pc.hp() == full - 3, "positive damage applies");
    pc.takeDamage(9999);
    CHECK(pc.hp() == 0, "damage clamps at zero");
    pc.restoreAll();
    CHECK(pc.hp() == full, "restoreAll refills HP");

    pc.spendResource(1);
    CHECK(pc.resource() == res - 1, "resource spend applies");
    pc.spendResource(-4);
    CHECK(pc.resource() == res - 1, "negative spend is ignored");
    pc.restoreResource(-3);
    CHECK(pc.resource() == res - 1, "negative restore is ignored");
    pc.restoreResource(9999, pc.stats(v).maxResource);
    CHECK(pc.resource() == pc.stats(v).maxResource, "restore clamps at the cap");

    CHECK(resourceRegenPerTurn(ClassId::Mage) == 2, "mage regens 2 resource/turn");
    CHECK(resourceRegenPerTurn(ClassId::Warrior) == 1, "warrior regens 1");
    CHECK(resourceRegenPerTurn(ClassId::Rogue) == 1, "rogue regens 1");
    CHECK(resourceRegenPerTurn(ClassId::Paladin) == 1, "paladin regens 1");

    Character pc2(ClassId::Warrior);
    CHECK(pc2.spendPoint(-1, 0) == false, "negative branch rejected");
    CHECK(pc2.spendPoint(0, -1) == false, "negative depth rejected");
    CHECK(pc2.spendPoint(3, 0) == false, "out-of-range branch rejected");
    CHECK(pc2.spendPoint(0, 4) == false, "out-of-range depth rejected");
    CHECK(pc2.pointsSpent() == 0, "invalid spends cost nothing");

    const Spell& fire = classSpells(ClassId::Mage)[0];
    CHECK(pc.canCast(fire), "full mana can cast");
    pc.spendResource(9999);
    CHECK(pc.canCast(fire) == false, "empty mana cannot cast");

    Character pc3(ClassId::Warrior);
    pc3.gainXp(pc3.xpToNext(), 0);
    CHECK(pc3.level() == 2, "exactly one level from one bar of xp");
    CHECK(pc3.spendPoint(0, 0), "learn a spell");
    pc3.setFloor(9);
    pc3.setHardcore(true);
    CHECK(pc3.train(TrainId::Might), "train one point");
    CHECK(pc3.trainRank(TrainId::Might) == 1, "rank tracks training");
    const auto snap = pc3.snapshot();
    CHECK(pc3.trainingSpent() == 1, "trainingSpent sums ranks");
    pc3.restoreAll();
    pc3.takeDamage(5);
    CHECK(pc3.hp() < snap.hp, "state mutated before restore");
    pc3.restore(snap);
    CHECK(pc3.level() == 2 && pc3.hp() == snap.hp && pc3.currentFloor() == 9 &&
          pc3.hardcore() && pc3.isUnlocked(0, 0) && pc3.trainRank(TrainId::Might) == 1,
          "snapshot/restore reproduces the full state");
}

// ---------------------------------------------------------------------------
// harness: item helpers and costs
// ---------------------------------------------------------------------------

void testItemsCostsAndHelpers() {
    CHECK(tierUnlockFloor(ItemTier::Iron) == 1, "iron from floor 1");
    CHECK(tierUnlockFloor(ItemTier::Steel) == 10, "steel from floor 10");
    CHECK(tierUnlockFloor(ItemTier::Mythril) == 24, "mythril from floor 24");
    CHECK(tierUnlockFloor(ItemTier::Adamant) == 45, "adamant from floor 45");
    CHECK(tierUnlockFloor(ItemTier::Void) == 75, "void from floor 75");

    Item rare;
    rare.consumable = false;
    rare.rarity = Rarity::Rare;
    rare.tier = ItemTier::Steel;
    rare.iLvl = 24;
    rare.slot = Slot::Weapon;
    CHECK(upgradeCost(rare) == 5 * (static_cast<int>(ItemTier::Steel) + 1) + 24 / 2,
          "upgrade cost = 5*(tier+1) + iLvl/2");
    CHECK(reforgeCost(rare) == 25 * (static_cast<int>(Rarity::Rare) + 1) * (1 + 24 / 8),
          "reforge cost = 25*(rarity+1)*(1+iLvl/8)");
    CHECK(awakenCost(rare) == 50 * (static_cast<int>(Rarity::Rare) + 1) * (1 + 24 / 8),
          "awaken cost = 50*(rarity+1)*(1+iLvl/8)");

    CHECK(canUpgrade(rare, 10) == false, "at-or-over floor cannot upgrade");
    CHECK(canUpgrade(rare, 30) == true, "below floor can upgrade");
    const Item pot = makeVendorPotion(1, false);
    CHECK(canUpgrade(pot, 100) == false, "potions never upgrade");

    core::Rng rng(7);
    std::set<SetId> floor5;
    for (int i = 0; i < 200; ++i) floor5.insert(rollSet(5, rng));
    CHECK(floor5.size() == 1 && *floor5.begin() == SetId::None,
          "floor 5 never rolls a set");
    std::map<SetId, int> counts;
    for (int i = 0; i < 600; ++i) ++counts[rollSet(12, rng)];
    CHECK(counts[SetId::Titanic] > 0 && counts[SetId::Infernal] == 0 &&
          counts[SetId::Frostbound] == 0 && counts[SetId::Voidwalk] == 0,
          "floor 12 unlocks only Titanic among sets");
}

void testItemStringRoundtrip() {
    core::Rng rng(21);
    const std::string path = "test_item_string.rpg";
    CHECK(save::erase(path) == false, "erase on a missing file fails cleanly");
    for (int i = 0; i < 60; ++i) {
        const Item g = makeGear(40, rng);
        const Item t = makeTrinket(40, rng);
        CHECK(save::stringToItem(save::itemToString(g)) == g, "gear string roundtrip");
        CHECK(save::stringToItem(save::itemToString(t)) == t, "trinket string roundtrip");
    }
    Item runed;
    runed.name = "Test Rune Blade";
    runed.slot = Slot::Weapon;
    runed.rarity = Rarity::Legendary;
    runed.consumable = false;
    runed.tier = ItemTier::Mythril;
    runed.iLvl = 40;
    runed.power = 30;
    runed.extraSockets = 1;
    runed.affixes.push_back(Affix{ "fiery", AffixType::DamagePct, 12 });
    runed.runes.push_back(makeRune(40, rng));
    CHECK(save::stringToItem(save::itemToString(runed)) == runed, "runed item string roundtrip");
    Item stack = makeVendorPotion(1, false);
    stack.count = 7;
    CHECK(save::stringToItem(save::itemToString(stack)) == stack, "potion stack string roundtrip");

    Character pc(ClassId::Warrior);
    Vault v;
    v.items.push_back(makeGear(5, rng));
    CHECK(save::write(path, pc, v), "item-save write succeeds");
    CHECK(save::peek(path).valid, "peek valid after write");
    CHECK(save::erase(path), "erase removes the file");
    CHECK(save::peek(path).valid == false, "peek invalid after erase");
}

// ---------------------------------------------------------------------------
// combat affixes & statuses
// ---------------------------------------------------------------------------

void testCursedAffix() {
    using namespace combat;
    Character pc(ClassId::Warrior);
    Vault v;
    Item sword;
    sword.slot = Slot::Weapon;
    sword.power = 40;
    v.add(sword);
    v.equip(0);
    const EffectiveStats st = pc.stats(v);
    Enemy e;
    e.name = "Stoneskin";
    e.hpMax = e.hp = 1000;
    e.attack = 0;
    e.defense = 0;
    e.xpReward = 1;
    e.goldReward = 1;
    e.affixes.push_back(EnemyAffix::Cursed);
    core::Rng rng(5);
    int adr = 0;
    const int dealt = strikeOnce(pc, st, e, rng, adr);
    CHECK(dealt > 0, "cursed strike lands");
    CHECK(pc.hp() == st.maxHp - dealt * 35 / 100, "cursed thorns reflect exactly 35%");
}

void testVampiricAffix() {
    using namespace combat;
    struct Run {
        bool won;
        std::string text;
    };
    const auto run = [](bool vamp) {
        Character pc(ClassId::Warrior);
        while (pc.level() < 10) pc.gainXp(pc.xpToNext(), 0);
        Vault v;
        Enemy e;
        e.name = "Rex";
        e.hpMax = e.hp = 80;
        e.attack = 20;
        e.defense = 0;
        e.critChance = 0;
        e.xpReward = 1;
        e.goldReward = 1;
        if (vamp) e.affixes.push_back(EnemyAffix::Vampiric);
        core::Rng rng(2026);
        IoFixture io(repeat("a\n", 6));
        const Result r = fight(pc, v, { e }, rng);
        return Run{ r.won, io.text() };
    };
    const Run plain = run(false);
    const Run vamped = run(true);
    CHECK(plain.won, "plain fight finishes");
    CHECK(vamped.won, "vampiric fight finishes");
    const auto plainReads = enemyHpReadouts(plain.text, "Rex");
    const auto vampReads = enemyHpReadouts(vamped.text, "Rex");
    const auto hits = enemyHits(vamped.text);
    CHECK(plainReads.size() >= 2 && vampReads.size() >= 2 && !hits.empty(),
          "parsed enough hp readouts and hits");
    CHECK(plainReads[0] == 80 && vampReads[0] == 80, "round 1 opens at full hp");
    CHECK(vampReads[1] == plainReads[1] + hits[0] * 30 / 100,
          "vampiric heals exactly 30% of the damage it deals");
}

void testRegeneratingAffix() {
    using namespace combat;
    Character pc(ClassId::Warrior);
    Vault v;
    Enemy e;
    e.name = "Mold";
    e.hpMax = e.hp = 30;
    e.attack = 1;
    e.defense = 0;
    e.critChance = 0;
    e.xpReward = 1;
    e.goldReward = 1;
    e.affixes.push_back(EnemyAffix::Regenerating);
    core::Rng rng(99);
    IoFixture io(repeat("a\n", 6));
    const Result r = fight(pc, v, { e }, rng);
    CHECK(r.won, "regenerating fight finishes");
    const std::string text = io.text();
    const auto reads = enemyHpReadouts(text, "Mold");
    const auto strikes = playerStrikes(text, "Mold");
    CHECK(reads.size() >= 2 && !strikes.empty(), "parsed hp and strikes");
    CHECK(countSubstr(text, "Mold regenerates 2 HP.") >= 1, "regen message appears");
    CHECK(reads[0] == 30, "opens at full hp");
    CHECK(reads[1] == 30 - strikes[0] + 2, "regen restores hpMax/15 after the first exchange");
}

void testBerserkerAffix() {
    using namespace combat;
    struct Run {
        bool won;
        int hit;
    };
    const auto run = [](int startHp) {
        Character pc(ClassId::Warrior);
        while (pc.level() < 10) pc.gainXp(pc.xpToNext(), 0);
        Vault v;
        Item plate;
        plate.slot = Slot::Armor;
        plate.power = 88;
        v.add(plate);
        v.equip(0);
        Enemy e;
        e.name = "Bruiser";
        e.hpMax = 200;
        e.hp = startHp;
        e.attack = 10;
        e.defense = 0;
        e.critChance = 0;
        e.xpReward = 1;
        e.goldReward = 1;
        e.affixes.push_back(EnemyAffix::Berserker);
        core::Rng rng(88);
        IoFixture io(repeat("a\n", 12));
        const Result r = fight(pc, v, { e }, rng);
        const auto hs = enemyHits(io.text());
        return Run{ r.won, hs.empty() ? 0 : hs[0] };
    };
    const Run full = run(200);
    const Run low = run(30);
    CHECK(full.won && low.won, "berserker fights are winnable");
    CHECK(full.hit > 0 && low.hit > 0, "berserker lands hits in both states");
    CHECK(low.hit >= 2 * full.hit - 1 && low.hit <= 2 * full.hit + 3,
          "berserker roughly doubles damage at low hp");
}

void testArmoredAffix() {
    using namespace combat;
    bool foundArmored = false, foundPlain = false;
    for (int seed = 1; seed <= 6000 && (!foundArmored || !foundPlain); ++seed) {
        core::Rng rng(static_cast<std::uint64_t>(seed));
        const Enemy b = makeBoss(16, rng);
        if (b.has(EnemyAffix::Armored)) {
            foundArmored = true;
            CHECK(b.defense == 10, "armored doubles boss base defense");
        } else if (!foundPlain) {
            foundPlain = true;
            CHECK(b.defense == 5, "boss base defense is floor/3");
        }
    }
    CHECK(foundArmored, "armored affix appears within the seed sweep");
}

void testSlowStatus() {
    using namespace combat;
    struct Run {
        bool won;
        int hit;
    };
    const auto run = [](bool frost) {
        Character pc(ClassId::Mage);
        while (pc.level() < 10) pc.gainXp(pc.xpToNext(), 0);
        CHECK(pc.spendPoint(frost ? 1 : 0, 0), "learn the single spell");
        Vault v;
        Enemy e;
        e.name = "Brute";
        e.hpMax = e.hp = 60;
        e.attack = 12;
        e.defense = 5;
        e.critChance = 0;
        e.xpReward = 1;
        e.goldReward = 1;
        core::Rng rng(33);
        IoFixture io(std::string("s\n1\n") + repeat("a\n", 20));
        const Result r = fight(pc, v, { e }, rng);
        const auto hs = enemyHits(io.text());
        return Run{ r.won, hs.empty() ? 0 : hs[0] };
    };
    const Run normal = run(false);  // Firebolt, no slow
    const Run slowed = run(true);   // Frostbolt, slow
    CHECK(normal.won && slowed.won, "slow-status fights are winnable");
    CHECK(normal.hit > 0 && slowed.hit > 0, "enemy lands hits in both fights");
    CHECK(normal.hit >= 2 * slowed.hit - 1 && normal.hit <= 2 * slowed.hit + 2,
          "slow halves the enemy's next attack");
}

void testArmorShred() {
    using namespace combat;
    const auto swing = [](bool corrosive) {
        Character pc(ClassId::Rogue);
        pc.gainXp(pc.xpToNext(), 0);
        CHECK(pc.spendPoint(1, 0), "rogue opens Venom Blade");
        if (corrosive) CHECK(pc.spendPoint(1, 1), "rogue learns Corrosive Slash");
        Vault v;
        Item sword;
        sword.slot = Slot::Weapon;
        sword.power = 20;
        v.add(sword);
        v.equip(0);
        Enemy e;
        e.name = "Statue";
        e.hpMax = e.hp = 300;
        e.attack = 1;
        e.defense = 40;
        e.critChance = 0;
        e.xpReward = 1;
        e.goldReward = 1;
        core::Rng rng(44);
        const std::string input = (corrosive ? std::string("s\n2\n") : std::string("s\n1\n"))
                                + repeat("a\n", 30);
        IoFixture io(input);
        const Result r = fight(pc, v, { e }, rng);
        CHECK(r.won, "armor-shred fight finishes");
        const auto ss = playerStrikes(io.text(), "Statue");
        return ss.empty() ? 0 : ss[0];
    };
    const int plain = swing(false);
    const int shredded = swing(true);
    CHECK(plain > 0 && shredded > 0, "both swings land");
    CHECK(shredded >= plain + 4, "armor shred meaningfully bypasses defense");
}

void testVulnerable() {
    using namespace combat;
    const auto swing = [](bool hex) {
        Character pc(ClassId::Mage);
        while (pc.level() < 10) pc.gainXp(pc.xpToNext(), 0);
        CHECK(pc.spendPoint(0, 0), "learn Firebolt");
        if (hex) {
            CHECK(pc.spendPoint(2, 0), "arcane branch 1");
            CHECK(pc.spendPoint(2, 1), "arcane branch 2");
            CHECK(pc.spendPoint(2, 2), "arcane branch 3");
            CHECK(pc.spendPoint(2, 3), "learn Hex");
        }
        Vault v;
        Item staff;
        staff.slot = Slot::Weapon;
        staff.power = 30;
        v.add(staff);
        v.equip(0);
        Enemy e;
        e.name = "Toad";
        e.hpMax = e.hp = 200;
        e.attack = 1;
        e.defense = 0;
        e.critChance = 0;
        e.xpReward = 1;
        e.goldReward = 1;
        core::Rng rng(66);
        const std::string input = (hex ? std::string("s\n5\n") : std::string("s\n1\n"))
                                + repeat("a\n", 30);
        IoFixture io(input);
        const Result r = fight(pc, v, { e }, rng);
        CHECK(r.won, "vulnerable fight finishes");
        const auto ss = playerStrikes(io.text(), "Toad");
        return ss.empty() ? 0 : ss[0];
    };
    const int plain = swing(false);
    const int vuln = swing(true);
    CHECK(plain > 0 && vuln > 0, "both fights produce a follow-up swing");
    CHECK(vuln >= plain + plain * 30 / 100, "vulnerable adds the full 30%");
}

void testBeltHotkey() {
    using namespace combat;
    {
        Character pc(ClassId::Warrior);
        Vault v;
        Item h = makeVendorPotion(1, false);
        h.count = 2;
        v.addPotion(h);
        v.bindBelt(0, PotionKind::Healing);
        Enemy e;
        e.name = "Mook";
        e.hpMax = e.hp = 30;
        e.attack = 1;
        e.xpReward = 1;
        e.goldReward = 1;
        core::Rng rng(12);
        IoFixture io(std::string("1\n") + repeat("a\n", 20));
        const Result r = fight(pc, v, { e }, rng);
        CHECK(r.won, "belt-sip fight finishes");
        CHECK(v.items.size() == 1 && v.items[0].count == 1, "one potion charge consumed");
        CHECK(io.contains("Restored"), "sip prints the restored message");
    }
    {
        Character pc(ClassId::Warrior);
        Vault v;
        Enemy e;
        e.name = "Mook";
        e.hpMax = e.hp = 30;
        e.attack = 1;
        e.xpReward = 1;
        e.goldReward = 1;
        core::Rng rng(12);
        IoFixture io(std::string("1\n") + repeat("a\n", 20));
        const Result r = fight(pc, v, { e }, rng);
        CHECK(r.won, "empty-belt fight finishes");
        CHECK(io.contains("has nothing to drink."), "empty belt slot reports nothing to drink");
    }
}

void testElementCombatText() {
    using namespace combat;
    const auto castAt = [](core::Element align) {
        Character pc(ClassId::Mage);
        while (pc.level() < 10) pc.gainXp(pc.xpToNext(), 0);
        CHECK(pc.spendPoint(0, 0), "learn Firebolt");
        Vault v;
        Item staff;
        staff.slot = Slot::Weapon;
        staff.power = 30;
        v.add(staff);
        v.equip(0);
        Enemy e;
        e.name = "Golem";
        e.hpMax = e.hp = 150;
        e.attack = 1;
        e.xpReward = 1;
        e.goldReward = 1;
        e.align = align;
        core::Rng rng(5);
        IoFixture io(std::string("s\n1\n") + repeat("a\n", 20));
        const Result r = fight(pc, v, { e }, rng);
        CHECK(r.won, "element-text fight finishes");
        return io.text();
    };
    CHECK(castAt(core::Element::Frost).find("CRUSHES the Frost foe") != std::string::npos,
          "Firebolt dominates a Frost foe");
    CHECK(castAt(core::Element::Fire).find("fizzles against the Fire foe") != std::string::npos,
          "Firebolt fizzles against a Fire foe");
}

// ---------------------------------------------------------------------------
// runes, perks, set bonuses
// ---------------------------------------------------------------------------

void testRuneforgeRoll() {
    bool sawTwo = false;
    for (int seed = 1; seed <= 200; ++seed) {
        core::Rng a(seed), b(seed);
        const auto plain = rollRunestoneLoot(50, false, a);
        const auto boost = rollRunestoneLoot(50, false, b, true);
        CHECK(boost.size() >= plain.size(), "boosted never drops fewer runestones");
        if (boost.size() == 2) sawTwo = true;
    }
    CHECK(sawTwo, "boosted occasionally drops two runestones");
    core::Rng ba(3), bb(3);
    const auto bp = rollRunestoneLoot(20, true, ba);
    const auto boosted = rollRunestoneLoot(20, true, bb, true);
    CHECK(bp.size() == 1 && boosted.size() == 2, "boss baseline 1 runestone, boosted 2");
}

void testRuneStatEffects() {
    Character pc(ClassId::Warrior);
    while (pc.level() < 10) pc.gainXp(pc.xpToNext(), 0);
    Vault empty;
    const EffectiveStats base = pc.stats(empty);
    const auto statsWith = [&](Rune r) {
        Vault v;
        Item t;
        t.slot = Slot::Trinket;
        t.rarity = Rarity::Legendary;
        t.consumable = false;
        t.iLvl = 50;
        t.power = 0;
        t.runes.push_back(r);
        v.add(t);
        v.equip(0);
        return pc.stats(v);
    };
    CHECK(statsWith({ RuneType::Warding, 10 }).defense == base.defense + base.defense * 10 / 100,
          "Warding adds % defense");
    CHECK(statsWith({ RuneType::Flow, 2 }).manaRegenPerTurn == base.manaRegenPerTurn + 2,
          "Flow adds flat mana regen");
    CHECK(statsWith({ RuneType::Finesse, 5 }).critChance == base.critChance + 5,
          "Finesse adds flat crit chance");
    CHECK(statsWith({ RuneType::Power, 8 }).attack == base.attack + base.attack * 8 / 100,
          "Power adds % attack");
    CHECK(statsWith({ RuneType::Vitality, 12 }).maxHp == base.maxHp + base.maxHp * 12 / 100,
          "Vitality adds % max HP");
}

void testPerkLeechingBulwark() {
    Character pc(ClassId::Warrior);
    Vault v;
    const auto base = pc.stats(v);
    v.perks[static_cast<std::size_t>(PerkId::Leeching)] = true;
    CHECK(pc.stats(v).lifeStealPct == base.lifeStealPct + 2, "Leeching adds 2% lifesteal");
    v.perks[static_cast<std::size_t>(PerkId::Bulwark)] = true;
    CHECK(pc.stats(v).defense == base.defense + 6, "Bulwark adds +6 defense");
}

void testSetBonusTable() {
    Character pc(ClassId::Warrior);
    while (pc.level() < 12) pc.gainXp(pc.xpToNext(), 0);
    const auto mk = [](Slot sl, SetId s) {
        Item it;
        it.slot = sl;
        it.consumable = false;
        it.rarity = Rarity::Common;
        it.iLvl = 1;
        it.power = 0;
        it.setTag = s;
        return it;
    };
    const auto st = [&](const std::vector<Item>& items) {
        Vault v;
        for (const auto& it : items) {
            v.add(it);
            v.equip(static_cast<int>(v.items.size()) - 1);
        }
        return pc.stats(v);
    };
    const EffectiveStats base = pc.stats(Vault{});

    const auto one = st({ mk(Slot::Weapon, SetId::Titanic) });
    CHECK(one.attack == base.attack && one.maxHp == base.maxHp && one.defense == base.defense,
          "one set piece grants no bonus");

    const auto t2 = st({ mk(Slot::Weapon, SetId::Titanic), mk(Slot::Armor, SetId::Titanic) });
    CHECK(t2.maxHp == base.maxHp + base.maxHp * 15 / 100, "Titanic 2pc +15% max HP");
    CHECK(t2.regenPerTurn == base.regenPerTurn, "Titanic 2pc does not regen");
    const auto t3 = st({ mk(Slot::Weapon, SetId::Titanic), mk(Slot::Armor, SetId::Titanic),
                         mk(Slot::Ring, SetId::Titanic) });
    CHECK(t3.maxHp == base.maxHp + base.maxHp * 15 / 100, "Titanic 3pc keeps the 2pc HP");
    CHECK(t3.regenPerTurn == base.regenPerTurn + 3, "Titanic 3pc +3 regen/turn");

    const auto i2 = st({ mk(Slot::Weapon, SetId::Infernal), mk(Slot::Armor, SetId::Infernal) });
    CHECK(i2.attack == base.attack + base.attack * 12 / 100, "Infernal 2pc +12% attack");
    CHECK(i2.critBonus == base.critBonus, "Infernal 2pc does not touch crit bonus");
    const auto i3 = st({ mk(Slot::Weapon, SetId::Infernal), mk(Slot::Armor, SetId::Infernal),
                         mk(Slot::Ring, SetId::Infernal) });
    CHECK(i3.critBonus == base.critBonus + 20, "Infernal 3pc +20 crit bonus");

    const auto f2 = st({ mk(Slot::Weapon, SetId::Frostbound), mk(Slot::Armor, SetId::Frostbound) });
    CHECK(f2.maxResource == base.maxResource + base.maxResource * 10 / 100,
          "Frostbound 2pc +10% max resource");
    const auto f3 = st({ mk(Slot::Weapon, SetId::Frostbound), mk(Slot::Armor, SetId::Frostbound),
                         mk(Slot::Ring, SetId::Frostbound) });
    CHECK(f3.manaRegenPerTurn == base.manaRegenPerTurn + 3, "Frostbound 3pc +3 mana regen");

    const auto v2 = st({ mk(Slot::Weapon, SetId::Voidwalk), mk(Slot::Armor, SetId::Voidwalk) });
    CHECK(v2.defense == base.defense + base.defense * 10 / 100, "Voidwalk 2pc +10% defense");
    const auto v3 = st({ mk(Slot::Weapon, SetId::Voidwalk), mk(Slot::Armor, SetId::Voidwalk),
                         mk(Slot::Ring, SetId::Voidwalk) });
    CHECK(v3.lifeStealPct == base.lifeStealPct + 3, "Voidwalk 3pc +3% lifesteal");

    const auto mixed = st({ mk(Slot::Weapon, SetId::Titanic), mk(Slot::Armor, SetId::Titanic),
                            mk(Slot::Ring, SetId::Voidwalk) });
    CHECK(mixed.maxHp == base.maxHp + base.maxHp * 15 / 100, "mixed 2+1 grants the Titanic 2pc");
    CHECK(mixed.defense == base.defense, "mixed does not trigger the Voidwalk 2pc");
}

// ---------------------------------------------------------------------------
// registry
// ---------------------------------------------------------------------------

struct Entry {
    const char* name;
    void (*fn)();
};

static const Entry kTests[] = {
    { "testRng", testRng },
    { "testFormulas", testFormulas },
    { "testRarityDistribution", testRarityDistribution },
    { "testReforgeKeepIdentity", testReforgeKeepIdentity },
    { "testGearTypeVariety", testGearTypeVariety },
    { "testUpgradeCap", testUpgradeCap },
    { "testAwaken", testAwaken },
    { "testSpellTree", testSpellTree },
    { "testStatsGear", testStatsGear },
    { "testPotion", testPotion },
    { "testSaveRoundtrip", testSaveRoundtrip },
    { "testSaveCorrupt", testSaveCorrupt },
    { "testEnemyGen", testEnemyGen },
    { "testAffixDistinctness", testAffixDistinctness },
    { "testDeathPreservesVault", testDeathPreservesVault },
    { "testDeathByFight", testDeathByFight },
    { "testThreatScalingGentle", testThreatScalingGentle },
    { "testElementTable", testElementTable },
    { "testRunestoneHelpers", testRunestoneHelpers },
    { "testRunestoneEconomy", testRunestoneEconomy },
    { "testMasteryCompounding", testMasteryCompounding },
    { "testMasteryRunestones", testMasteryRunestones },
    { "testHealScaling", testHealScaling },
    { "testAspectScaling", testAspectScaling },
    { "testBossLootEpic", testBossLootEpic },
    { "testSaveV4RecordsAndRunes", testSaveV4RecordsAndRunes },
    { "testBasicAttackLoop", testBasicAttackLoop },
    { "testRogueFirstStrikeSmoke", testRogueFirstStrikeSmoke },
    { "testBossSummonSafe", testBossSummonSafe },
    { "testDotExpiry", testDotExpiry },
    { "testStunSkipsEnemyTurn", testStunSkipsEnemyTurn },
    { "testLevelUpPreservesHp", testLevelUpPreservesHp },
    { "testXpAppliedOnce", testXpAppliedOnce },
    { "testVendorPotion", testVendorPotion },
    { "testEnemyHitCap", testEnemyHitCap },
    { "testPassiveRegenApplied", testPassiveRegenApplied },
    { "testMerchantNoFreeMaterials", testMerchantNoFreeMaterials },
    { "testSetDropsReachable", testSetDropsReachable },
    { "testDoTFinishTakesVictory", testDoTFinishTakesVictory },
    { "testFleeCountsSlainEnemies", testFleeCountsSlainEnemies },
    { "testEnemyCritApplied", testEnemyCritApplied },
    { "testBossNameOrder", testBossNameOrder },
    { "testSaveWriteAtomic", testSaveWriteAtomic },
    { "testHeirloomExplorationGold", testHeirloomExplorationGold },
    { "testTrainingStats", testTrainingStats },
    { "testAddMergesPotions", testAddMergesPotions },
    { "testPotionStack", testPotionStack },
    { "testBeltKind", testBeltKind },
    { "testVersion9Roundtrip", testVersion9Roundtrip },
    { "testVersion10Roundtrip", testVersion10Roundtrip },
    { "testGloryTrack", testGloryTrack },
    { "testAdrenalineKeptOnPhase", testAdrenalineKeptOnPhase },
    { "testSpellBlurbScaling", testSpellBlurbScaling },
    { "testBeltDedupe", testBeltDedupe },
    { "testTrapDodge", testTrapDodge },
    { "testDisplayWidth", testDisplayWidth },
    { "testAskIntTrim", testAskIntTrim },
    { "testMasteryCap", testMasteryCap },
    { "testRadiantReflect", testRadiantReflect },
    { "testSummonerAffix", testSummonerAffix },
    { "testHexerDebuff", testHexerDebuff },
    { "testNewFloorEvents", testNewFloorEvents },
    { "testPerkStats", testPerkStats },
    { "testGreedLoot", testGreedLoot },
    { "testEvasionPerk", testEvasionPerk },
    { "testPerkSaveRoundtrip", testPerkSaveRoundtrip },
    { "testTrinketPowers", testTrinketPowers },
    { "testTrinketCombat", testTrinketCombat },
    { "testPaladinClass", testPaladinClass },
    { "testHardcorePermadeath", testHardcorePermadeath },
    { "testAmbienceSweep", testAmbienceSweep },
    { "testUiLayoutPlain", testUiLayoutPlain },
    { "testQuitSaveFlush", testQuitSaveFlush },
    { "testRngApi", testRngApi },
    { "testCharacterEdges", testCharacterEdges },
    { "testItemsCostsAndHelpers", testItemsCostsAndHelpers },
    { "testItemStringRoundtrip", testItemStringRoundtrip },
    { "testCursedAffix", testCursedAffix },
    { "testVampiricAffix", testVampiricAffix },
    { "testRegeneratingAffix", testRegeneratingAffix },
    { "testBerserkerAffix", testBerserkerAffix },
    { "testArmoredAffix", testArmoredAffix },
    { "testSlowStatus", testSlowStatus },
    { "testArmorShred", testArmorShred },
    { "testVulnerable", testVulnerable },
    { "testBeltHotkey", testBeltHotkey },
    { "testElementCombatText", testElementCombatText },
    { "testRuneforgeRoll", testRuneforgeRoll },
    { "testRuneStatEffects", testRuneStatEffects },
    { "testPerkLeechingBulwark", testPerkLeechingBulwark },
    { "testSetBonusTable", testSetBonusTable },
};

} // namespace

int main(int argc, char** argv) {
    std::string filter;
    bool listOnly = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--list") listOnly = true;
        else if (a == "--filter") { if (i + 1 < argc) filter = argv[++i]; }
        else if (a.rfind("--filter=", 0) == 0) filter = a.substr(9);
    }

    std::remove("glory.rpg");
    int matched = 0;
    int failedTests = 0;
    for (const Entry& t : kTests) {
        if (!filter.empty() && std::string(t.name).find(filter) == std::string::npos) continue;
        ++matched;
        if (listOnly) { std::cout << t.name << "\n"; continue; }
        const int before = failures;
        g_currentTest = t.name;
        t.fn();
        g_currentTest = "";
        if (failures > before) {
            ++failedTests;
            std::cout << "[FAILED] " << t.name << "\n";
        }
    }
    if (listOnly) {
        std::cout << "\n" << matched << " tests listed\n";
        return 0;
    }
    std::remove("glory.rpg");

    std::cout << "\n" << checks << " checks, " << failures << " failures ("
              << matched << " tests run, " << failedTests << " failing)\n";
    return failures == 0 ? 0 : 1;
}