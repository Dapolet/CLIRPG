#include "character.hpp"
#include "combat.hpp"
#include "core.hpp"
#include "items.hpp"
#include "save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
        reforge(it, rng);
        CHECK(it.slot == slot, "reforge keeps slot");
        CHECK(it.rarity == rarity, "reforge keeps rarity");
        CHECK(it.iLvl == ilvl, "reforge keeps iLvl");
        CHECK(static_cast<int>(it.affixes.size()) == na, "reforge keeps affix count");
        CHECK(!it.name.empty(), "reforge regens name");
    }
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
    CHECK(canAwaken(rare), "rare with 1 affix can awaken");
    rare.affixes.push_back(Affix{ "Fierce", AffixType::DamagePct, 5 });
    CHECK(!canAwaken(rare), "rare with 2 affixes fully awakened");
    awaken(rare, rng);
    CHECK(rare.affixes.size() == 2, "awaken respects slot cap");

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

void testAutoAttackToggle() {
    using namespace combat;
    core::Rng rng(1);
    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());

    // easy kill: '=' should run the fight with no further input
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
        std::istringstream in("=\n");
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        const Result res = fight(pc, v, { weak }, rng);
        std::cin.rdbuf(oldIn);
        CHECK(res.won, "auto-attack finishes an easy fight");
        CHECK(pc.hp() == pc.stats(v).maxHp, "rogue untouchable in a trivial fight");
    }

    // dangerous fight: auto must cede control before death
    {
        Character pc(ClassId::Rogue);
        Vault v;
        out.str("");
        Enemy mean;
        mean.name = "Brute";
        mean.hpMax = mean.hp = 300;
        mean.attack = 20;
        mean.xpReward = 3;
        mean.goldReward = 3;
        std::istringstream in("=\n");
        std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
        const Result res = fight(pc, v, { mean }, rng);
        std::cin.rdbuf(oldIn);
        CHECK(out.str().find("Auto-attack stopped") != std::string::npos,
              "auto-attack prints a stop notice when it gets dangerous");
        CHECK(!res.won, "player eventually falls");
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

} // namespace

int main() {
    testRng();
    testFormulas();
    testRarityDistribution();
    testReforgeKeepIdentity();
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
    testAutoAttackToggle();
    testRogueFirstStrikeSmoke();
    testBossSummonSafe();
    testDotExpiry();
    testXpAppliedOnce();
    testVendorPotion();

    std::cout << "\n" << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}