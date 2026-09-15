#pragma once

#include <map>
#include <string>

namespace rpg {

// Persistent kill/visit records. Keyed by display name so future content
// (new monsters, bosses, biome changes) never invalidates old saves.
struct Bestiary {
    std::map<std::string, int> enemies;   // monster name  → kills
    std::map<std::string, int> bosses;    // boss name     → kills
    std::map<std::string, int> affixes;   // affix name    → times seen
    std::map<std::string, int> biomes;    // biome name    → times visited

    void addEnemy(const std::string& n) { ++enemies[n]; }
    void addBoss(const std::string& n)  { ++bosses[n]; }
    void addAffix(const std::string& n) { ++affixes[n]; }
    void addBiome(const std::string& n) { ++biomes[n]; }

    int enemyKills(const std::string& n) const { const auto it = enemies.find(n); return it == enemies.end() ? 0 : it->second; }
    int bossKills(const std::string& n)  const { const auto it = bosses.find(n);  return it == bosses.end()  ? 0 : it->second; }

    int totalKills() const {
        int t = 0;
        for (const auto& kv : enemies) t += kv.second;
        return t;
    }
    int totalBosses() const {
        int t = 0;
        for (const auto& kv : bosses) t += kv.second;
        return t;
    }
};

} // namespace rpg