#pragma once

#include "character.hpp"
#include "core.hpp"
#include "items.hpp"

#include <string>
#include <vector>

namespace rpg::ui {

// Semantic ANSI color tokens — the one place hue→meaning is decided.
namespace c {
inline constexpr int accent = 36;   // structure: panels, chips, floors
inline constexpr int soft   = 90;   // muted: brackets, hints, helper text
inline constexpr int neutral = 37;  // neutral text / item names
inline constexpr int good   = 32;   // positive: hp, equipped, gains
inline constexpr int gold   = 33;   // economy: gold, xp, amber narration
inline constexpr int bad    = 31;   // danger: damage, hp loss, bosses
inline constexpr int arcane = 35;   // essence, masteries, runes, epic
inline constexpr int mana   = 34;   // mana, frost, mage, information
inline constexpr int shine  = 96;   // bright cyan: lifesteal, highlights
inline constexpr int frost  = 94;   // bright blue: frost, glow effects
inline constexpr int flare  = 91;   // bright red: adrenaline, extraction
}

// Rich glyphs with guaranteed 7-bit ASCII fallbacks for plain mode / pipes.
struct Glyph {
    const char* rich;   // UTF-8 TTY glyph
    const char* plain;  // 7-bit ASCII fallback
};

inline constexpr Glyph G_H      = { "\u2500", "-" };
inline constexpr Glyph G_LT     = { "\u256d", "+" };
inline constexpr Glyph G_RT     = { "\u256e", "+" };
inline constexpr Glyph G_LB     = { "\u2570", "+" };
inline constexpr Glyph G_RB     = { "\u256f", "+" };
inline constexpr Glyph G_VR     = { "\u2502", "|" };
inline constexpr Glyph G_WALL   = { "\u2588", "#" };
inline constexpr Glyph G_METER  = { "\u2591", "." };
inline constexpr Glyph G_GEM    = { "\u25c6", "o" };
inline constexpr Glyph G_DI     = { "\u25b8", ">" };
inline constexpr Glyph G_CK     = { "\u2713", "+" };
inline constexpr Glyph G_SK     = { "\u25cb", "o" };
inline constexpr Glyph G_MED    = { "\u25c9", "*" };
inline constexpr Glyph G_STAIRS = { "\u25bc", "v" };
inline constexpr Glyph G_SWORD  = { "\u2694", "/" };
inline constexpr Glyph G_BOSS   = { "\u2620", "X" };

// item-type icons
inline constexpr Glyph G_SHIELD  = { "🛡", "[]" };
inline constexpr Glyph G_HELM    = { "🪖", "H" };
inline constexpr Glyph G_GLOVES  = { "🧤", "()" };
inline constexpr Glyph G_BOOTS   = { "👢", ">" };
inline constexpr Glyph G_RING    = { "💍", "o" };
inline constexpr Glyph G_AMULET  = { "📿", "&" };
inline constexpr Glyph G_POTION  = { "🧪", "!" };
inline constexpr Glyph G_RUNE    = { "💠", "R" };
inline constexpr Glyph G_SOCKET  = { "◆", "*" };
inline constexpr Glyph G_EMPTY   = { "◇", "." };
inline constexpr Glyph G_GOLD    = { "🪙", "$" };
inline constexpr Glyph G_SHARD   = { "🔷", "#" };
inline constexpr Glyph G_ESSENCE = { "✨", "*" };
inline constexpr Glyph G_TRAP    = { "⚠", "!" };
inline constexpr Glyph G_TOME    = { "📜", "?" };
inline constexpr Glyph G_SHRINE  = { "🗿", "&" };
inline constexpr Glyph G_CHEST   = { "🎁", "#" };
inline constexpr Glyph G_BED     = { "🛏", "o" };
inline constexpr Glyph G_ANVIL   = { "⚒", "+" };
inline constexpr Glyph G_NOTE    = { "📖", "N" };

const char* gly(const Glyph& g);    // g.rich when color/TTY, g.plain otherwise

std::string color(int code, const std::string& text);
std::string bold(const std::string& text);
std::string dim(const std::string& text);
int rarityColor(Rarity r);

// block meters, e.g.  ████████░░
std::string meter(int value, int max, int width, int code);
std::string hpMeter(int hp, int max, int width);   // auto green→yellow→red

// decorative rules & panels
void printHeader();                       // title banner
void panelTop(const std::string& title, int code = c::accent);
void panelLine(const std::string& line);
void panelBottom(int code = c::accent);

// colored cue glyphs
std::string chip(int n);                      // " [n] "
std::string gold(int amount);                 // " 42g "
std::string shards(int amount);
int potionCount(const Vault& vault, PotionKind k);               // " 12 shards "
std::string elementTag(core::Element e);      // colored "[Fire]" or ""

// terminal awareness & plain mode
int  terminalWidth();                          // ioctl → COLUMNS → 80
bool isTty();                                  // true when stdout is a terminal
bool colorEnabled();                           // NO_COLOR / TERM=dumb / piped → false
void setForcePlain(bool plain);                // --plain / tests
int  layoutWidth();                            // clamped panel width (44..100)
std::string wrap(const std::string& text, int width);   // word wrap, hard-splits long words
int displayWidth(const std::string& text);               // UTF-8 terminal column count
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

// new-game run mode: 1 softcore (classic) / 2 hardcore (permadeath)
bool chooseMode();

// pick one Rift Aspect perk (once per Ascension, permanent).
void choosePerk(Vault& vault);

// full camp loop. Descend / Quit / Ascend (Rift Aspect, floor 100+).
CampResult camp(Character& pc, Vault& vault, core::Rng& rng);

// shared helpers used by camp & tests-of-UI-free logic
void showCharacter(const Character& pc, const Vault& vault);
void showVault(const Vault& vault, const std::vector<int>& gear);
void equipMenu(Character& pc, Vault& vault);
void spellBook(const Character& pc, const Vault& vault);
void recordsScreen(const Character& pc, const Vault& vault);
void inventoryMenu(const Character& pc, Vault& vault);
void beltMenu(Vault& vault);
bool merchantMenu(Character& pc, Vault& vault, core::Rng& rng);

} // namespace rpg::ui