#pragma once

#include "character.hpp"
#include "items.hpp"

#include <array>
#include <string>
#include <vector>

namespace rpg::glory {

constexpr int kVersion = 1;
constexpr int kNumAchievements = 15;

// The account-wide Glory track: achievements are evaluated from the
// character+vault snapshot each time they are re-checked, and once earned
// they stay earned even after the slot is overwritten. Held in glory.rpg.
struct Fallen {
    ClassId cls = ClassId::Warrior;
    int level = 0;
    int floor = 0;
    int kills = 0;
    long long goldEarned = 0;
};

struct Data {
    std::array<bool, kNumAchievements> unlocked{};
    std::vector<Fallen> fallen;
};

Data load();                                    // "glory.rpg"; fresh Data on any problem
bool save(const Data& d);                       // checksummed "GLORY v1"
int  unlockedCount(const Data& d);

// Re-evaluate the character+vault against every achievement; print the newly
// earned lines and persist when anything changed. Safe to call anywhere.
void sync(const Character& pc, const Vault& vault);

// Record a permadeath: append a Fallen entry and unlock "Pay the Iron Price".
void fall(const Character& pc, const Vault& vault);

const char* achievementName(int i);             // 0..kNumAchievements-1

} // namespace rpg::glory