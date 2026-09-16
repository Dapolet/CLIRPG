#include "character.hpp"
#include "combat.hpp"
#include "core.hpp"
#include "game.hpp"
#include "glory.hpp"
#include "items.hpp"
#include "save.hpp"
#include "ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

#define CHECK(c, msg)                                             \
    do {                                                          \
        ++checks;                                                \
        if (!(c)) {                                               \
            ++failures;                                           \
            std::cout << "FAIL(line " << __LINE__ << "): " << msg << "\n"; \
        }                                                         \
    } while (0)

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
    core::Rng rng(7);
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
    (void)rng;
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
    for (auto cls : { ClassId::Warrior, ClassId::Mage, ClassId::Rogue }) {
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
    CHECK(spellHeal(bastion, 1) == 10 + 1, "Bastion scales 1.2/level");
    CHECK(spellHeal(bastion, 10) == 10 + 12, "Bastion scales 1.2/level");
    CHECK(spellHeal(rally, 1) == 8 + 1, "Rally scales 1.0/level");
    CHECK(spellHeal(rally, 10) == 8 + 10, "Rally scales 1.0/level");

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
        CHECK(items.size() == 2, "boss drops two pieces");
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
    const std::uint64_t seed = 16;   // first floorEvent draw is case 1 (Forgotten Cache)
    const int floor = 5;

    core::Rng plainRng(seed);
    Character plainPc(ClassId::Warrior);
    Vault plain;
    const int beforePlain = plain.gold;
    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    ui::floorEvent(plainPc, plain, floor, plainRng);
    std::cout.rdbuf(oldOut);
    const int plainGain = plain.gold - beforePlain;

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

    CHECK(plainGain > 0, "seed lands on a gold-granting floor event");
    CHECK(boostedGain == plainGain + plainGain * 20 / 100,
          "Heirloom boosts exploration gold by 20%");
    CHECK(plain.totalGoldEarned == plainGain && boosted.totalGoldEarned == boostedGain,
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

    CHECK(save::write(path, pc, v), "write v9 save");
    Character pc2(ClassId::Warrior);
    Vault v2;
    CHECK(save::read(path, &pc2, &v2), "read v9 save");
    CHECK(pc2.snapshot().training == pc.snapshot().training, "training ranks roundtrip");
    CHECK(pc2.hardcore(), "hardcore flag roundtrips");
    CHECK(v2.belt == v.belt, "belt kinds roundtrip");
    int healing = 0;
    for (const auto& it : v2.items)
        if (it.potionKind == PotionKind::Healing) healing += it.count;
    CHECK(healing == 3, "healing stack count roundtrips");
    int mana = 0;
    for (const auto& it : v2.items)
        if (it.potionKind == PotionKind::Mana) mana += it.count;
    CHECK(mana == 2, "mana stack count roundtrips");

    // legacy header must be rejected (save-format bump rule)
    {
        std::ifstream in(path);
        std::string all((std::istreambuf_iterator<char>(in)), {});
        all.replace(0, all.find('\n'), "RPGSAVE v8");
        std::ofstream out(path, std::ios::trunc);
        out << all;
    }
    Character pc3(ClassId::Warrior);
    Vault v3;
    CHECK(!save::read(path, &pc3, &v3), "v8 header rejected by v9 reader");
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

} // namespace

int main() {
    std::remove("glory.rpg");
    testRng();
    testFormulas();
    testRarityDistribution();
    testReforgeKeepIdentity();
    testGearTypeVariety();
    testUpgradeCap();
    testAwaken();
    testSpellTree();
    testStatsGear();
    testPotion();
    testSaveRoundtrip();
    testSaveCorrupt();
    testEnemyGen();
    testAffixDistinctness();
    testDeathPreservesVault();
    testDeathByFight();
    testThreatScalingGentle();
    testElementTable();
    testRunestoneHelpers();
    testRunestoneEconomy();
    testMasteryCompounding();
    testMasteryRunestones();
    testHealScaling();
    testAspectScaling();
    testBossLootEpic();
    testSaveV4RecordsAndRunes();
    testBasicAttackLoop();
    testRogueFirstStrikeSmoke();
    testBossSummonSafe();
    testDotExpiry();
    testStunSkipsEnemyTurn();
    testLevelUpPreservesHp();
    testXpAppliedOnce();
    testVendorPotion();
    testEnemyHitCap();
    testPassiveRegenApplied();
    testMerchantNoFreeMaterials();
    testSetDropsReachable();
    testDoTFinishTakesVictory();
    testFleeCountsSlainEnemies();
    testEnemyCritApplied();
    testBossNameOrder();
    testSaveWriteAtomic();
    testHeirloomExplorationGold();
    testTrainingStats();
    testAddMergesPotions();
    testPotionStack();
    testBeltKind();
    testVersion9Roundtrip();
    testGloryTrack();
    testHardcorePermadeath();
    testAmbienceSweep();
    testUiLayoutPlain();
    testQuitSaveFlush();
    std::remove("glory.rpg");

    std::cout << "\n" << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}