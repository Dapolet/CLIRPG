#pragma once

#include "character.hpp"
#include "core.hpp"
#include "items.hpp"

#include <string>
#include <vector>

namespace rpg::combat {

enum class EnemyAffix {
    None, Vampiric, Regenerating, Armored, Ethereal, Berserker, Cursed,
    Radiant, Hexer, Summoner,
};
const char* enemyAffixName(EnemyAffix a);

struct Enemy {
    std::string name;
    bool boss = false;
    bool elite = false;
    bool summonUsed = false;
    int hpMax = 0;
    int hp = 0;
    int attack = 0;
    int defense = 0;
    int critChance = 0;
    int xpReward = 0;
    int goldReward = 0;
    std::vector<EnemyAffix> affixes;
    core::Element align = core::Element::None;

    bool alive() const { return hp > 0; }
    bool has(EnemyAffix a) const;
};

// A player's summoned ally (e.g. Necromancer's "Arise, Skeleton Knight").
// Lives for a single fight: auto-attacks each round and absorbs enemy hits.
struct Minion {
    std::string name;
    int hpMax = 0;
    int hp = 0;
    int attack = 0;
    int defense = 0;

    bool alive() const { return hp > 0; }
};

struct Loot {
    int gold = 0;
    int shards = 0;
    int essence = 0;
    std::vector<Item> items;
    std::vector<Rune> runes;
};

struct Result {
    bool won = false;
    bool fled = false;
    int xp = 0;
    int kills = 0;
    Loot loot;
};

// One full turn-based fight. Player always acts first.
Result fight(Character& pc, Vault& vault, std::vector<Enemy> enemies, core::Rng& rng);

// Encounter / boss generation. aspect = Rift Aspect prestige level.
std::vector<Enemy> makeEncounter(int floor, core::Rng& rng, int aspect = 0);
Enemy makeBoss(int floor, core::Rng& rng, int aspect = 0);

bool resolveSummon(Enemy& e, std::vector<Enemy>& enemies);

int strikeOnce(Character& pc, const EffectiveStats& st, Enemy& e, core::Rng& rng,
               int& adrenaline, int critExtra = 0);
int castOnce(Character& pc, const EffectiveStats& st, const Spell& s, Enemy& e,
             core::Rng& rng, int& adrenaline, int critExtra = 0);

// Named zone for a floor band (display + bestiary record).
const char* biomeFor(int floor);
const char* biomeLoreFor(int floor);

} // namespace rpg::combat