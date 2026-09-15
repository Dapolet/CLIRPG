#pragma once

#include "character.hpp"
#include "core.hpp"
#include "items.hpp"

#include <string>
#include <vector>

namespace rpg::ui {

std::string color(int code, const std::string& text);
std::string bold(const std::string& text);
std::string dim(const std::string& text);
int rarityColor(Rarity r);

// block meters, e.g.  ████████░░
std::string meter(int value, int max, int width, int code);
std::string hpMeter(int hp, int max, int width);   // auto green→yellow→red

// decorative rules & panels
void printHeader();                       // title banner
void rule(int code = 90);                 // thin divider line
void titleBar(const std::string& title, int code = 36);
void panelTop(const std::string& title, int code = 36);
void panelLine(const std::string& line);
void panelBottom(int code = 36);

// colored cue glyphs
std::string chip(int n);                      // " [n] "
std::string gold(int amount);                 // " 42g "
std::string elementTag(core::Element e);      // colored "[Fire]" or ""

// terminal awareness & plain mode
int  terminalWidth();                          // ioctl → COLUMNS → 80
bool isTty();                                  // true when stdout is a terminal
bool colorEnabled();                           // NO_COLOR / TERM=dumb / piped → false
void setForcePlain(bool plain);                // --plain / tests
std::string stripAnsi(const std::string& text);
std::string center(const std::string& text, int width);

enum class CampResult { Descend, Quit, Ascend };

// Random pre-fight floor event (fountain, cache, rune-fall, trap, apothecary).
void floorEvent(Character& pc, Vault& vault, int floor, core::Rng& rng);

// main menu: returns 1 new / 2 continue / 3 quit
int mainMenu();

// pick one of the 3 slots; `newGame` asks for overwrite confirmation.
int chooseSaveSlot(bool newGame);
void saveSlotsList();

// new-game class select
ClassId chooseClass();

// pick one Rift Aspect perk (once per Ascension, permanent).
void choosePerk(Vault& vault);

// full camp loop. Descend / Quit / Ascend (Rift Aspect, floor 100+).
CampResult camp(Character& pc, Vault& vault, core::Rng& rng);

// shared helpers used by camp & tests-of-UI-free logic
void showCharacter(const Character& pc, const Vault& vault);
void showVault(const Vault& vault);
void equipMenu(Character& pc, Vault& vault);
void spellBook(const Character& pc);
void recordsScreen(const Character& pc, const Vault& vault);
void manageMenu(Character& pc, Vault& vault);
void beltMenu(Vault& vault);

} // namespace rpg::ui