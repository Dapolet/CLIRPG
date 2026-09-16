#include "glory.hpp"

#include "ui.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace rpg::glory {

namespace {

const char* kPath = "glory.rpg";

unsigned char checksum(const std::string& s) {
    unsigned char c = 0;
    for (const char ch : s) c ^= static_cast<unsigned char>(ch);
    return c;
}

std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);
    while (std::getline(is, cur, d)) out.push_back(cur);
    return out;
}

bool wouldEarn(const Character& pc, const Vault& v, int i) {
    switch (i) {
        case 0:  return v.kills >= 1;
        case 1:  return v.bossesSlain >= 1;
        case 2:  return v.bestFloor >= 25;
        case 3:  return v.bestFloor >= 50;
        case 4:  return v.bestFloor >= 100;
        case 5:  return v.legendaryFound >= 1;
        case 6:  return v.deaths >= 1;
        case 7:  return v.mastery >= 3;
        case 8:  return v.aspect >= 1;
        case 9:  return v.kills >= 100;
        case 10: return v.totalGoldEarned >= 10000;
        case 11: return pc.hardcore();
        case 12: return pc.hardcore() && v.bestFloor >= 25;
        case 13: return pc.hardcore() && v.bestFloor >= 50;
        default: return false;   // 14 (Pay the Iron Price) is granted by fall()
    }
}

bool writeFile(const Data& d) {
    std::ostringstream body;
    body << "GLORY v" << kVersion << "\n";
    body << "unlocked=";
    for (const bool b : d.unlocked) body << (b ? '1' : '0');
    body << "\n";
    body << "fallen=" << d.fallen.size() << "\n";
    for (const auto& f : d.fallen)
        body << "fallen=" << static_cast<int>(f.cls) << "|" << f.level << "|"
             << f.floor << "|" << f.kills << "|" << f.goldEarned << "\n";
    const std::string text = body.str();
    std::ofstream out(kPath, std::ios::trunc);
    if (!out) return false;
    out << text;
    out << "[sig]\n";
    out << static_cast<int>(checksum(text)) << "\n";
    return out.good();
}

// Mark every achievement the character+vault currently fulfills; prints the
// newly earned lines. Returns true when any new unlock happened.
bool syncInto(Data& d, const Character& pc, const Vault& vault) {
    bool changed = false;
    for (int i = 0; i < kNumAchievements; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        if (!d.unlocked[idx] && wouldEarn(pc, vault, i)) {
            d.unlocked[idx] = true;
            changed = true;
            std::cout << ui::color(ui::c::gold, ui::bold("Achievement unlocked")) << ": "
                      << achievementName(i) << "  ("
                      << unlockedCount(d) << "/" << kNumAchievements << ")\n";
        }
    }
    return changed;
}

} // namespace

Data load() {
    Data d;
    std::ifstream in(kPath);
    if (!in) return d;
    std::ostringstream all;
    all << in.rdbuf();
    const std::string data = all.str();

    const std::string sigTag = "[sig]\n";
    const auto sigPos = data.rfind(sigTag);
    if (sigPos == std::string::npos) return d;
    const std::string body = data.substr(0, sigPos);
    const int sig = std::atoi(data.substr(sigPos + sigTag.size()).c_str());
    if (sig != static_cast<int>(checksum(body))) return d;
    if (body.rfind("GLORY v" + std::to_string(kVersion), 0) != 0) return d;

    std::istringstream is(body);
    std::string line;
    while (std::getline(is, line)) {
        if (line.rfind("unlocked=", 0) == 0) {
            const std::string bits = line.substr(9);
            for (std::size_t i = 0; i < d.unlocked.size() && i < bits.size(); ++i)
                d.unlocked[i] = bits[i] == '1';
        } else if (line.rfind("fallen=", 0) == 0 && line.size() > 7) {
            const auto f = split(line.substr(7), '|');
            if (f.size() < 5) continue;
            const int cls = std::atoi(f[0].c_str());
            if (cls < 0 || cls >= static_cast<int>(ClassId::Rogue) + 1) continue;
            Fallen fe;
            fe.cls = static_cast<ClassId>(cls);
            fe.level = std::atoi(f[1].c_str());
            fe.floor = std::atoi(f[2].c_str());
            fe.kills = std::atoi(f[3].c_str());
            fe.goldEarned = std::atoll(f[4].c_str());
            d.fallen.push_back(fe);
        }
    }
    return d;
}

bool save(const Data& d) { return writeFile(d); }

int unlockedCount(const Data& d) {
    int n = 0;
    for (const bool b : d.unlocked)
        if (b) ++n;
    return n;
}

void sync(const Character& pc, const Vault& vault) {
    Data d = load();
    if (syncInto(d, pc, vault)) writeFile(d);
}

void fall(const Character& pc, const Vault& vault) {
    Data d = load();
    const bool earned = syncInto(d, pc, vault);
    Fallen f;
    f.cls = pc.classId();
    f.level = pc.level();
    f.floor = pc.currentFloor();
    f.kills = vault.kills;
    f.goldEarned = vault.totalGoldEarned;
    d.fallen.push_back(f);
    if (!d.unlocked[14]) {
        d.unlocked[14] = true;
        std::cout << ui::color(ui::c::bad, ui::bold("Achievement unlocked")) << ": "
                  << achievementName(14) << "  ("
                  << unlockedCount(d) << "/" << kNumAchievements << ")\n";
    }
    if (earned || d.unlocked[14]) writeFile(d);
}

const char* achievementName(int i) {
    switch (i) {
        case 0:  return "First Blood";
        case 1:  return "Boss Slayer";
        case 2:  return "Deep Delver";
        case 3:  return "Dungeon Master";
        case 4:  return "Riftbreaker";
        case 5:  return "Legendary Hunter";
        case 6:  return "Death Is a Teacher";
        case 7:  return "Master of the Rift";
        case 8:  return "Aspect of Eternity";
        case 9:  return "Slayer";
        case 10: return "Midas";
        case 11: return "Hardcore Heart";
        case 12: return "From the Ashes";
        case 13: return "Immortal";
        case 14: return "Pay the Iron Price";
        default: return "?";
    }
}

} // namespace rpg::glory