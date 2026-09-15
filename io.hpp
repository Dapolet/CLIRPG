#pragma once

#include <string>

namespace rpg::io {

// Single shared input helpers for ui and combat. Prompts loop until valid.
std::string readLine(const std::string& prompt);
int askInt(const std::string& prompt, int lo, int hi);

} // namespace rpg::io