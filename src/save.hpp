#pragma once

#include "character.hpp"
#include "items.hpp"

#include <string>

namespace rpg::save {

constexpr int kVersion = 9;

bool write(const std::string& path, const Character& pc, Vault& vault);
bool read(const std::string& path, Character* pc, Vault* vault);
bool erase(const std::string& path);   // permadeath: remove the save file

struct SaveSummary {
    bool valid = false;
    ClassId cls = ClassId::Warrior;
    int level = 1;
    int floor = 1;
    bool hardcore = false;
    int version = kVersion;
    long long saveTime = 0;
};
SaveSummary peek(const std::string& path);

// Soft death: loses 20% gold, drops one floor, restores fully.
// The Vault (items, equipped gear, runes, materials) is intentionally untouched.
void applyDeath(Character& pc, Vault& vault);

// Serialization helpers (also used by tests).
std::string itemToString(const Item& it);
Item stringToItem(const std::string& s);

} // namespace rpg::save