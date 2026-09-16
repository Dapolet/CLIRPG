#include "save.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <vector>

namespace rpg::save {

namespace {

std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);
    while (std::getline(is, cur, d)) out.push_back(cur);
    return out;
}

std::string joinInts(const std::array<int, kNumSlots>& a) {
    std::string out;
    for (int i = 0; i < kNumSlots; ++i) {
        if (i) out += " ";
        out += std::to_string(a[static_cast<std::size_t>(i)]);
    }
    return out;
}

void parseInts(const std::string& s, std::array<int, kNumSlots>& a) {
    std::istringstream is(s);
    for (int i = 0; i < kNumSlots; ++i) {
        if (!(is >> a[static_cast<std::size_t>(i)])) return;
    }
}

bool kv(const std::string& line, const std::string& key, std::string* v) {
    if (line.rfind(key, 0) != 0) return false;
    *v = line.substr(key.size());
    return true;
}

void parseBeast(std::map<std::string, int>& m, const std::string& s) {
    const auto p = s.find('|');
    if (p == std::string::npos) return;
    m[s.substr(0, p)] = std::stoi(s.substr(p + 1));
}

const char* tag(int i) { return i ? "1" : "0"; }

unsigned char checksum(const std::string& s) {
    unsigned char c = 0;
    for (const char ch : s) c ^= static_cast<unsigned char>(ch);
    return c;
}

} // namespace

std::string itemToString(const Item& it) {
    std::string o = it.name;
    o += std::string("|") + tag(it.consumable);
    o += "|" + std::to_string(static_cast<int>(it.slot));
    o += "|" + std::to_string(static_cast<int>(it.tier));
    o += "|" + std::to_string(static_cast<int>(it.rarity));
    o += "|" + std::to_string(it.iLvl);
    o += "|" + std::to_string(it.power);
    o += "|" + std::to_string(it.heal);
    o += "|" + std::to_string(it.manaRestore);
    o += "|" + std::to_string(it.uid);
    o += "|" + std::to_string(static_cast<int>(it.setTag));
    o += "|" + std::to_string(it.affixes.size());
    for (const auto& a : it.affixes)
        o += "|" + a.prefix + "|" + std::to_string(static_cast<int>(a.type)) + "|" + std::to_string(a.value);
    o += "|" + std::to_string(it.runes.size());
    for (const auto& r : it.runes)
        o += "|" + std::to_string(static_cast<int>(r.type)) + "|" + std::to_string(r.value);
    o += "|" + std::to_string(static_cast<int>(it.potionKind)) + "|" + std::to_string(it.count);
    return o;
}

Item stringToItem(const std::string& s) {
    const auto f = split(s, '|');
    Item it;
    if (f.size() < 12) return it;
    it.name = f[0];
    it.consumable = f[1] == "1";
    it.slot = static_cast<Slot>(std::stoi(f[2]));
    it.tier = static_cast<ItemTier>(std::stoi(f[3]));
    it.rarity = static_cast<Rarity>(std::stoi(f[4]));
    it.iLvl = std::stoi(f[5]);
    it.power = std::stoi(f[6]);
    it.heal = std::stoi(f[7]);
    it.manaRestore = std::stoi(f[8]);
    it.uid = std::stoi(f[9]);
    it.setTag = static_cast<SetId>(std::stoi(f[10]));
    const int n = std::stoi(f[11]);
    std::size_t k = 12;
    for (int i = 0; i < n && k + 2 < f.size(); ++i) {
        Affix a;
        a.prefix = f[k];
        ++k;
        a.type = static_cast<AffixType>(std::stoi(f[k]));
        ++k;
        a.value = std::stoi(f[k]);
        ++k;
        it.affixes.push_back(a);
    }
    if (k < f.size()) {
        const int nr = std::stoi(f[k]);
        ++k;
        for (int i = 0; i < nr && k + 1 < f.size(); ++i) {
            Rune r;
            r.type = static_cast<RuneType>(std::stoi(f[k]));
            ++k;
            r.value = std::stoi(f[k]);
            ++k;
            it.runes.push_back(r);
        }
    }
    if (k + 1 < f.size()) {
        it.potionKind = static_cast<PotionKind>(std::stoi(f[k]));
        it.count = std::stoi(f[k + 1]);
    }
    return it;
}

bool write(const std::string& path, const Character& pc, Vault& vault) {
    vault.saveTime = static_cast<std::int64_t>(std::time(nullptr));
    const auto snap = pc.snapshot();
    std::ostringstream body;

    body << "RPGSAVE v" << kVersion << "\n";
    body << "[character]\n";
    body << "class=" << static_cast<int>(snap.id) << "\n";
    body << "level=" << snap.level << "\n";
    body << "xp=" << snap.xp << "\n";
    body << "skillPoints=" << snap.skillPoints << "\n";
    body << "hp=" << snap.hp << "\n";
    body << "resource=" << snap.resource << "\n";
    body << "floor=" << snap.floor << "\n";
    body << "unlocked=";
    for (int i = 0; i < 12; ++i) body << tag(snap.unlocked[static_cast<std::size_t>(i)]);
    body << "\n";
    body << "train=";
    for (int i = 0; i < kNumTrains; ++i) {
        if (i) body << " ";
        body << snap.training[static_cast<std::size_t>(i)];
    }
    body << "\n";
    body << "hardcore=" << tag(snap.hardcore) << "\n";

    body << "[vault]\n";
    body << "gold=" << vault.gold << "\n";
    body << "shards=" << vault.shards << "\n";
    body << "essence=" << vault.essence << "\n";
    body << "bestFloor=" << vault.bestFloor << "\n";
    body << "bossesSlain=" << vault.bossesSlain << "\n";
    body << "kills=" << vault.kills << "\n";
    body << "deaths=" << vault.deaths << "\n";
    body << "legendaryFound=" << vault.legendaryFound << "\n";
    body << "mastery=" << vault.mastery << "\n";
    body << "aspect=" << vault.aspect << "\n";
    body << "totalGoldEarned=" << vault.totalGoldEarned << "\n";
    body << "nextUid=" << vault.nextUid << "\n";
    body << "saveTime=" << vault.saveTime << "\n";
    body << "belt=" << vault.belt[0] << " " << vault.belt[1] << "\n";
    body << "perks=";
    for (int i = 0; i < static_cast<int>(PerkId::kNumPerks); ++i)
        body << tag(vault.perks[static_cast<std::size_t>(i)]);
    body << "\n";
    for (const auto& kv : vault.bestiary.enemies) body << "beast=" << kv.first << "|" << kv.second << "\n";
    for (const auto& kv : vault.bestiary.bosses)  body << "boss="  << kv.first << "|" << kv.second << "\n";
    for (const auto& kv : vault.bestiary.affixes) body << "affix=" << kv.first << "|" << kv.second << "\n";
    for (const auto& kv : vault.bestiary.biomes)  body << "biome=" << kv.first << "|" << kv.second << "\n";
    body << "equipped=" << joinInts(vault.equipped) << "\n";
    body << "items=" << vault.items.size() << "\n";
    for (const auto& it : vault.items) body << "item=" << itemToString(it) << "\n";
    body << "runes=" << vault.runes.size() << "\n";
    for (const auto& r : vault.runes)
        body << "rune=" << static_cast<int>(r.type) << "|" << r.value << "\n";

    const std::string text = body.str();
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << text;
    f << "[sig]\n";
    f << static_cast<int>(checksum(text)) << "\n";
    return f.good();
}

bool read(const std::string& path, Character* pc, Vault* vault) {
    try {
    std::ifstream f(path);
    if (!f) return false;
    std::ostringstream all;
    all << f.rdbuf();
    const std::string data = all.str();

    const std::string sigTag = "[sig]\n";
    const auto sigPos = data.rfind(sigTag);
    if (sigPos == std::string::npos) return false;
    const std::string body = data.substr(0, sigPos);
    const int sig = std::atoi(data.substr(sigPos + sigTag.size()).c_str());
    if (sig != static_cast<int>(checksum(body))) return false;

    if (body.rfind("RPGSAVE v" + std::to_string(kVersion), 0) != 0) return false;

    std::istringstream is(body);
    std::string line;
    bool inVault = false;

    Character::Snapshot snap{};
    Vault v;
    std::vector<std::string> itemLines;
    std::vector<std::string> runeLines;

    while (std::getline(is, line)) {
        if (line == "[character]") { inVault = false; continue; }
        if (line == "[vault]") { inVault = true; continue; }
        std::string val;
        if (inVault) {
            if (kv(line, "gold=", &val)) v.gold = std::stoi(val);
            else if (kv(line, "shards=", &val)) v.shards = std::stoi(val);
            else if (kv(line, "essence=", &val)) v.essence = std::stoi(val);
            else if (kv(line, "bestFloor=", &val)) v.bestFloor = std::stoi(val);
            else if (kv(line, "bossesSlain=", &val)) v.bossesSlain = std::stoi(val);
            else if (kv(line, "kills=", &val)) v.kills = std::stoi(val);
            else if (kv(line, "deaths=", &val)) v.deaths = std::stoi(val);
            else if (kv(line, "legendaryFound=", &val)) v.legendaryFound = std::stoi(val);
            else if (kv(line, "mastery=", &val)) v.mastery = std::stoi(val);
            else if (kv(line, "aspect=", &val)) v.aspect = std::stoi(val);
            else if (kv(line, "totalGoldEarned=", &val)) v.totalGoldEarned = std::stoi(val);
            else if (kv(line, "nextUid=", &val)) v.nextUid = std::stoi(val);
            else if (kv(line, "saveTime=", &val)) v.saveTime = std::stoll(val);
            else if (kv(line, "belt=", &val)) {
                std::istringstream isv(val);
                isv >> v.belt[0] >> v.belt[1];
            }
            else if (kv(line, "perks=", &val)) {
                for (int i = 0; i < static_cast<int>(PerkId::kNumPerks) && static_cast<std::size_t>(i) < val.size(); ++i)
                    v.perks[static_cast<std::size_t>(i)] = val[static_cast<std::size_t>(i)] == '1';
            }
            else if (kv(line, "beast=", &val)) parseBeast(v.bestiary.enemies, val);
            else if (kv(line, "boss=", &val))  parseBeast(v.bestiary.bosses, val);
            else if (kv(line, "affix=", &val)) parseBeast(v.bestiary.affixes, val);
            else if (kv(line, "biome=", &val)) parseBeast(v.bestiary.biomes, val);
            else if (kv(line, "equipped=", &val)) parseInts(val, v.equipped);
            else if (kv(line, "item=", &val)) itemLines.push_back(val);
            else if (kv(line, "rune=", &val)) runeLines.push_back(val);
        } else {
            if (kv(line, "class=", &val)) snap.id = static_cast<ClassId>(std::stoi(val));
            else if (kv(line, "level=", &val)) snap.level = std::stoi(val);
            else if (kv(line, "xp=", &val)) snap.xp = std::stoi(val);
            else if (kv(line, "skillPoints=", &val)) snap.skillPoints = std::stoi(val);
            else if (kv(line, "hp=", &val)) snap.hp = std::stoi(val);
            else if (kv(line, "resource=", &val)) snap.resource = std::stoi(val);
            else if (kv(line, "floor=", &val)) snap.floor = std::stoi(val);
            else if (kv(line, "unlocked=", &val)) {
                for (int i = 0; i < 12 && static_cast<std::size_t>(i) < val.size(); ++i)
                    snap.unlocked[static_cast<std::size_t>(i)] = val[static_cast<std::size_t>(i)] == '1';
            }
            else if (kv(line, "train=", &val)) {
                std::istringstream isv(val);
                for (int i = 0; i < kNumTrains; ++i) {
                    if (!(isv >> snap.training[static_cast<std::size_t>(i)])) break;
                }
            }
            else if (kv(line, "hardcore=", &val)) snap.hardcore = val == "1";
        }
    }

    for (const auto& il : itemLines) v.items.push_back(stringToItem(il));
    for (const auto& rl : runeLines) {
        const auto f = split(rl, '|');
        if (f.size() < 2) continue;
        Rune r;
        r.type = static_cast<RuneType>(std::stoi(f[0]));
        r.value = std::stoi(f[1]);
        v.runes.push_back(r);
    }

    *pc = Character(ClassId::Warrior);
    pc->restore(snap);
    *vault = v;
    return true;
    } catch (...) {
        return false;
    }
}

bool erase(const std::string& path) { return std::remove(path.c_str()) == 0; }

SaveSummary peek(const std::string& path) {
    SaveSummary s;
    Character pc(ClassId::Warrior);
    Vault v;
    if (!read(path, &pc, &v)) return s;
    s.valid = true;
    s.cls = pc.classId();
    s.level = pc.level();
    s.floor = pc.currentFloor();
    s.hardcore = pc.hardcore();
    s.version = kVersion;
    s.saveTime = v.saveTime;
    return s;
}

void applyDeath(Character& pc, Vault& vault) {
    vault.gold -= vault.gold / 5;
    ++vault.deaths;
    pc.setFloor(std::max(1, pc.currentFloor() - 1));
    const auto st = pc.stats(vault);
    pc.restoreAll(st.maxHp, st.maxResource);
}

} // namespace rpg::save