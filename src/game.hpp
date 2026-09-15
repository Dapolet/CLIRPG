#pragma once

#include "character.hpp"
#include "combat.hpp"
#include "core.hpp"
#include "items.hpp"
#include "ui.hpp"

#include <functional>
#include <string>

namespace rpg::game {

// The per-slot adventure loop: drives a character from its current floor
// through floor events, encounters, loot/XP, and the camp until the player
// quits, dies mid-floor, or ascends. `persist` is invoked after every
// state-changing moment (death, level-up, quit, ascend) so the caller stays
// crash-safe without the loop knowing what storage layer is backing it.
void adventure(Character& pc, Vault& vault, core::Rng& rng, const std::string& savePath,
               const std::function<void()>& persist = {});

} // namespace rpg::game