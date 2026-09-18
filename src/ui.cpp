#include "ui.hpp"

#include "glory.hpp"
#include "io.hpp"
#include "save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#ifndef _WIN32
#include <sys/ioctl.h>
#include <unistd.h>
#else
#include <io.h>
#endif

namespace rpg::ui {

const char* gly(const Glyph& g) { return colorEnabled() ? g.rich : g.plain; }

static const char* itemIcon(const Item& it) {
    if (it.isPot()) return gly(G_POTION);
    switch (it.slot) {
        case Slot::Weapon: return gly(G_SWORD);
        case Slot::Armor:  return gly(G_SHIELD);
        case Slot::Helm:   return gly(G_HELM);
        case Slot::Gloves: return gly(G_GLOVES);
        case Slot::Boots:  return gly(G_BOOTS);
        case Slot::Ring:   return gly(G_RING);
        case Slot::Amulet: return gly(G_AMULET);
        case Slot::Trinket: return gly(G_RING);
    }
    return gly(G_EMPTY);
}

static bool equippedItem(const Vault& vault, int idx) {
    for (int s = 0; s < kNumSlots; ++s)
        if (vault.equipped[static_cast<std::size_t>(s)] == idx) return true;
    return false;
}

int potionCount(const Vault& vault, PotionKind k) {
    int n = 0;
    for (const auto& it : vault.items)
        if (it.potionKind == k) n += std::max(it.count, 0);
    return n;
}

static bool slotInGroup(Slot s, int g) {
    switch (g) {
        case 1: return s == Slot::Weapon;
        case 2: return s == Slot::Armor || s == Slot::Helm ||
                       s == Slot::Gloves || s == Slot::Boots;
        case 3: return s == Slot::Ring || s == Slot::Amulet || s == Slot::Trinket;
    }
    return false;
}

namespace {

// Plain mode: color helpers become identity; unicode glyphs fall back to ASCII.
bool gForcePlain = false;

std::string h(int n) {
    const char* ch = gly(G_H);
    std::string s;
    s.reserve(3 * static_cast<std::size_t>(std::max(n, 0)));
    for (int i = 0; i < n; ++i) s += ch;
    return s;
}

void panelWrapped(const std::string& text, int width, const std::string& pad) {
    const std::string wrapped = wrap(text, width);
    std::size_t pos = 0;
    while (true) {
        const std::size_t nl = wrapped.find('\n', pos);
        panelLine(pad + wrapped.substr(pos, nl == std::string::npos
                                       ? std::string::npos : nl - pos));
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
}

bool yesNo(const std::string& prompt) {
    std::string line;
    std::cout << prompt << " [y/n]: ";
    std::getline(std::cin, line);
    return !line.empty() && (line[0] == 'y' || line[0] == 'Y');
}

std::string passiveName(ClassId c) {
    switch (c) {
        case ClassId::Warrior: return "Adrenaline";
        case ClassId::Mage:    return "Arcane Flow";
        case ClassId::Rogue:   return "First Strike";
        case ClassId::Paladin: return "Righteous Grace";
        case ClassId::Necromancer: return "Soul Harvest";
    }
    return "?";
}

std::string branchName(ClassId c, int b) {
    if (c == ClassId::Warrior) return b == 0 ? "Berserker" : b == 1 ? "Defender" : "Warcry";
    if (c == ClassId::Mage)    return b == 0 ? "Fire"      : b == 1 ? "Frost"    : "Arcane";
    if (c == ClassId::Paladin) return b == 0 ? "Holy"      : b == 1 ? "Order"    : "Light";
    if (c == ClassId::Necromancer)
        return b == 0 ? "Decay" : b == 1 ? "Death" : "Undeath";
    return                        b == 0 ? "Shadow"     : b == 1 ? "Poison"   : "Tricks";
}

bool sellMenu(Vault& vault) {
    std::vector<int> gear;
    for (std::size_t i = 0; i < vault.items.size(); ++i)
        if (!vault.items[i].isPot()) gear.push_back(static_cast<int>(i));
    if (gear.empty()) {
        std::cout << dim("  No gear to sell.\n");
        return false;
    }
    while (true) {
        std::cout << "\n";
        panelTop("Sell gear", c::gold);
        for (std::size_t k = 0; k < gear.size(); ++k) {
            const Item& it = vault.items[static_cast<std::size_t>(gear[k])];
            std::string tag = equippedItem(vault, gear[k])
                              ? color(c::good, " [equipped]") : std::string();
            panelLine(chip(static_cast<int>(k + 1))
                      + itemIcon(it) + " "
                      + color(rarityColor(it.rarity), it.name)
                      + tag
                      + "  " + gold(sellPrice(it)));
        }
        panelLine(dim(chip(0) + "back"));
        panelBottom(c::gold);
        const int pick = io::askInt("Sell which? (0 back)", 0, static_cast<int>(gear.size()));
        if (pick == 0) return false;
        const int idx = gear[static_cast<std::size_t>(pick - 1)];
        const int price = sellPrice(vault.items[static_cast<std::size_t>(idx)]);
        panelLine(color(c::gold, gly(G_DI))
                  + " Sold " + color(rarityColor(vault.items[static_cast<std::size_t>(idx)].rarity),
                                     vault.items[static_cast<std::size_t>(idx)].name)
                  + " for " + gold(price));
        std::cout << "\n";
        vault.gold += price;
        vault.removeAt(idx);
        gear.clear();
        for (std::size_t i = 0; i < vault.items.size(); ++i)
            if (!vault.items[i].isPot()) gear.push_back(static_cast<int>(i));
        if (gear.empty()) return false;
    }
}

bool salvageMenu(Vault& vault) {
    std::vector<int> gear;
    for (std::size_t i = 0; i < vault.items.size(); ++i)
        if (!vault.items[i].isPot()) gear.push_back(static_cast<int>(i));
    if (gear.empty()) {
        std::cout << dim("  No gear to salvage.\n");
        return false;
    }
    while (true) {
        std::cout << "\n";
        panelTop("Salvage gear", c::mana);
        for (std::size_t k = 0; k < gear.size(); ++k) {
            const Item& it = vault.items[static_cast<std::size_t>(gear[k])];
            std::string val = color(c::accent, std::to_string(salvageShards(it)) + " shards");
            if (salvageEssence(it) > 0) val += color(c::arcane, ", 1 essence");
            std::string tag = equippedItem(vault, gear[k])
                              ? color(c::good, " [equipped]") : std::string();
            panelLine(chip(static_cast<int>(k + 1))
                      + itemIcon(it) + " "
                      + color(rarityColor(it.rarity), it.name) + tag + "  " + val);
        }
        panelLine(dim(chip(0) + "back"));
        panelBottom(c::mana);
        const int pick = io::askInt("Salvage which? (0 back)", 0, static_cast<int>(gear.size()));
        if (pick == 0) return false;
        const int idx = gear[static_cast<std::size_t>(pick - 1)];
        vault.shards += salvageShards(vault.items[static_cast<std::size_t>(idx)]);
        vault.essence += salvageEssence(vault.items[static_cast<std::size_t>(idx)]);
        panelLine(color(c::accent, gly(G_DI)) + " Salvaged "
                  + color(rarityColor(vault.items[static_cast<std::size_t>(idx)].rarity),
                          vault.items[static_cast<std::size_t>(idx)].name));
        std::cout << "\n";
        vault.removeAt(idx);
        gear.clear();
        for (std::size_t i = 0; i < vault.items.size(); ++i)
            if (!vault.items[i].isPot()) gear.push_back(static_cast<int>(i));
        if (gear.empty()) return false;
    }
}

bool socketAction(Item& it, Vault& vault) {
    if (it.freeSockets() <= 0) {
        std::cout << dim("  No free sockets on this item.\n");
        return false;
    }
    if (vault.runes.empty()) {
        std::cout << dim("  You have no runestones to socket.\n");
        return false;
    }
    while (true) {
        std::cout << "\n";
        panelTop("Socket into " + it.name, c::arcane);
        panelLine(color(c::accent, "Free sockets: " + std::to_string(it.freeSockets())));
        for (std::size_t k = 0; k < vault.runes.size(); ++k)
            panelLine(chip(static_cast<int>(k + 1)) + color(c::arcane, vault.runes[k].describe()));
        panelLine(dim(chip(0) + "back"));
        panelBottom(c::arcane);
        const int pick = io::askInt("Socket which rune? (0 back)", 0,
                                    static_cast<int>(vault.runes.size()));
        if (pick == 0) return false;
        const Rune r = vault.runes[static_cast<std::size_t>(pick - 1)];
        if (socketRune(it, r)) {
            vault.runes.erase(vault.runes.begin() + (pick - 1));
            panelLine(color(c::arcane, gly(G_DI)) + " Socketed " + r.describe() + " into " + it.name);
            std::cout << "\n";
            return true;
        }
        std::cout << dim("  No free sockets.\n");
        return false;
    }
}

bool extractAction(Item& it, Vault& vault, int discountPct) {
    if (it.runes.empty()) {
        std::cout << dim("  Nothing socketed to extract.\n");
        return false;
    }
    const int cost = discountedCost(runeExtractCost(it), discountPct);
    if (vault.gold < cost) {
        std::cout << dim("  Extraction costs " + std::to_string(cost) + "g. Not enough gold.\n");
        return false;
    }
    vault.gold -= cost;
    const Rune r = extractRune(it);
    vault.runes.push_back(r);
    panelLine(color(c::arcane, gly(G_DI)) + " Extracted " + r.describe() + " (" + gold(cost) + ")");
    std::cout << "\n";
    return true;
}

} // namespace

int terminalWidth() {
#ifndef _WIN32
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return static_cast<int>(w.ws_col);
#endif
    if (const char* c = std::getenv("COLUMNS")) {
        const int v = std::atoi(c);
        if (v > 0) return v;
    }
    return 80;
}

bool isTty() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

bool colorEnabled() {
    if (gForcePlain) return false;
    if (std::getenv("NO_COLOR") != nullptr) return false;
    if (const char* t = std::getenv("TERM")) {
        const std::string term = t;
        if (term == "dumb") return false;
    }
    return isTty();
}

void setForcePlain(bool plain) { gForcePlain = plain; }

int layoutWidth() { return std::clamp(terminalWidth(), 44, 100); }

static int utf8Len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static std::uint32_t utf8Codepoint(const std::string& s, std::size_t i, int len) {
    std::uint32_t cp = static_cast<std::uint32_t>(
        static_cast<unsigned char>(s[i]) & (0xFFu >> (len + 1)));
    for (int k = 1; k < len && i + static_cast<std::size_t>(k) < s.size(); ++k)
        cp = (cp << 6) | (static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]) & 0x3Fu);
    return cp;
}

static int codepointWidth(std::uint32_t cp) {
    if (cp == 0) return 0;
    if ((cp >= 0x0300 && cp <= 0x036F) ||
        (cp >= 0x200B && cp <= 0x200F) || cp == 0xFEFF)
        return 0;
    if ((cp >= 0x1100 && cp <= 0x115F) ||
        (cp >= 0x2E80 && cp <= 0x303E) ||
        (cp >= 0x3041 && cp <= 0x33FF) ||
        (cp >= 0x3400 && cp <= 0x4DBF) ||
        (cp >= 0x4E00 && cp <= 0x9FFF) ||
        (cp >= 0xA000 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) ||
        (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE30 && cp <= 0xFE4F) ||
        (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) ||
        (cp >= 0x1F300 && cp <= 0x1FAFF) ||
        (cp >= 0x20000 && cp <= 0x3FFFD))
        return 2;
    return 1;
}

int displayWidth(const std::string& text) {
    int w = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const int len = utf8Len(static_cast<unsigned char>(text[i]));
        w += codepointWidth(utf8Codepoint(text, i, len));
        i += static_cast<std::size_t>(len);
    }
    return w;
}

std::string wrap(const std::string& text, int width) {
    if (width <= 0 || displayWidth(text) <= width) return text;
    std::string out;
    int col = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t start = i;
        while (i < text.size() && text[i] != ' ' && text[i] != '\n') ++i;
        const std::string word = text.substr(start, i - start);
        bool newline = false;
        if (i < text.size()) {
            if (text[i] == '\n') newline = true;
            ++i;
        }
        const int w = displayWidth(word);
        if (col > 0 && col + 1 + w > width) { out += '\n'; col = 0; }
        if (w > width) {
            for (std::size_t k = 0; k < word.size();) {
                if (col > 0) { out += '\n'; col = 0; }
                std::size_t take = 0;
                int taken = 0;
                while (k + take < word.size()) {
                    const int len = utf8Len(static_cast<unsigned char>(word[k + take]));
                    const int cw = codepointWidth(utf8Codepoint(word, k + take, len));
                    if (taken > 0 && taken + cw > width) break;
                    take += static_cast<std::size_t>(len);
                    taken += cw;
                }
                out += word.substr(k, take);
                k += take;
                col += taken;
            }
        } else {
            if (col > 0) { out += ' '; ++col; }
            out += word;
            col += w;
        }
        if (newline) { out += '\n'; col = 0; }
    }
    return out;
}

std::string center(const std::string& text, int width) {
    const int tw = displayWidth(text);
    if (width <= 0 || tw >= width) return text;
    const int pad = width - tw;
    std::string out;
    out.reserve(text.size() + static_cast<std::size_t>(pad));
    for (int i = 0; i < pad / 2; ++i) out += ' ';
    out += text;
    for (int i = pad / 2; i < pad; ++i) out += ' ';
    return out;
}

std::string color(int code, const std::string& text) {
    if (!colorEnabled()) return text;
    return "\x1b[" + std::to_string(code) + "m" + text + "\x1b[0m";
}

std::string bold(const std::string& text) {
    if (!colorEnabled()) return text;
    return "\x1b[1m" + text + "\x1b[0m";
}

std::string dim(const std::string& text) {
    if (!colorEnabled()) return text;
    return "\x1b[2m" + text + "\x1b[0m";
}

int rarityColor(Rarity r) {
    switch (r) {
        case Rarity::Common:    return c::neutral;
        case Rarity::Uncommon:  return c::good;
        case Rarity::Rare:      return c::accent;
        case Rarity::Epic:      return c::arcane;
        case Rarity::Legendary: return c::gold;
    }
    return c::neutral;
}

std::string meter(int value, int max, int width, int code) {
    if (width <= 0) width = 1;
    double pct = 0.0;
    if (max > 0) pct = static_cast<double>(value) / static_cast<double>(max);
    pct = std::clamp(pct, 0.0, 1.0);
    const int fill = std::clamp(static_cast<int>(std::lround(pct * static_cast<double>(width))),
                                0, width);
    const char* full = gly(G_WALL);
    const char* empty = gly(G_METER);
    std::string s;
    for (int i = 0; i < fill; ++i) s += full;
    for (int i = fill; i < width; ++i) s += empty;
    return color(code, s);
}

std::string hpMeter(int hp, int max, int width) {
    double pct = 0.0;
    if (max > 0) pct = static_cast<double>(hp) / static_cast<double>(max);
    int code = c::good;
    if (pct <= 0.25) code = c::bad;
    else if (pct <= 0.50) code = c::gold;
    return meter(hp, max, width, code);
}

void panelTop(const std::string& title, int code) {
    const int inner = layoutWidth() - 2;
    const int t = std::clamp(displayWidth(title), 0, std::max(inner - 4, 4));
    const int L = std::max(0, (inner - t) / 2 - 1);
    const int R = std::max(0, inner - t - L - 2);
    std::cout << color(code,
                       std::string(gly(G_LT)) + h(L) + " " + title + " " + h(R)
                       + std::string(gly(G_RT)))
              << "\n";
}

void panelLine(const std::string& line) {
    std::cout << color(c::accent, gly(G_VR)) << " " << line << "\n";
}

void panelBottom(int code) {
    std::cout << color(code, std::string(gly(G_LB)) + h(layoutWidth() - 2)
                       + std::string(gly(G_RB)))
              << "\n";
}

std::string chip(int n) {
    return color(c::soft, "[") + color(c::accent, std::to_string(n)) + color(c::soft, "] ");
}

std::string gold(int amount) {
    return color(c::gold, std::to_string(amount) + "g");
}

std::string shards(int amount) {
    return color(c::accent, std::to_string(amount) + " shards");
}

std::string elementTag(core::Element e) {
    switch (e) {
        case core::Element::Fire:   return color(c::bad, "[Fire]");
        case core::Element::Frost:  return color(c::frost, "[Frost]");
        case core::Element::Arcane: return color(c::arcane, "[Arcane]");
        default:                    return "";
    }
}

void printHeader() {
    const int w = layoutWidth();
    std::cout << color(c::accent, std::string(gly(G_LT)) + h(w - 2) + std::string(gly(G_RT))) << "\n";
    std::cout << color(c::accent, std::string(gly(G_VR)) + " "
                + center(std::string(gly(G_GEM)) + "  C L I R P G  " + gly(G_GEM), w - 4) + " "
                + std::string(gly(G_VR))) << "\n";
    std::cout << color(c::accent, std::string(gly(G_VR)) + " "
                + center("Endless Rift  \u2014  a CLI dungeon crawler", w - 4) + " "
                + std::string(gly(G_VR))) << "\n";
    std::cout << color(c::accent, std::string(gly(G_LB)) + h(w - 2) + std::string(gly(G_RB))) << "\n\n";
}

int mainMenu() {
    while (true) {
        if (!std::cin.good()) return 3;   // EOF → quit instead of spinning
        printHeader();
        panelTop("Main Menu", c::accent);
        panelLine(color(c::gold, chip(1)) + bold("New Game") + dim("   start a fresh descent"));
        panelLine(chip(2) + bold("Continue") + dim("         load a wanderer"));
        panelLine(chip(3) + bold("Quit") + dim("           leave the Rift"));
        panelBottom();
        const int c = io::askInt("Choose", 1, 3);
        if (!std::cin.good()) return 3;   // EOF → quit cleanly
        if (c != -1) return c;
    }
}

void saveSlotsList() {
    for (int i = 1; i <= 3; ++i) {
        const auto s = save::peek("save" + std::to_string(i) + ".rpg");
        if (!s.valid) {
            panelLine(chip(i) + dim("Slot " + std::to_string(i) + "  \u2014  empty"));
        } else {
            std::string line = chip(i)
                               + color(rarityColor(Rarity::Rare), className(s.cls))
                               + "  L" + std::to_string(s.level)
                               + color(c::accent, "  \u25b8 Floor " + std::to_string(s.floor));
            if (s.hardcore)
                line += color(c::bad, "  " + std::string(gly(G_BOSS)) + " hardcore");
            panelLine(line);
        }
    }
}

int chooseSaveSlot(bool newGame) {
    while (true) {
        std::cout << "\n";
        panelTop(newGame ? "New Game" : "Continue", c::accent);
        panelLine(dim(newGame ? "Choose the slot your legend begins in:"
                              : "Choose the wanderer you return to:"));
        saveSlotsList();
        panelLine(dim(chip(0) + "cancel"));
        panelBottom();
        const int slot = io::askInt("Slot", 0, 3);
        if (slot == 0) return 0;
        if (newGame) {
            const auto s = save::peek("save" + std::to_string(slot) + ".rpg");
            if (s.valid && !yesNo("That slot already has a character. Overwrite?")) continue;
        }
        return slot;
    }
}

ClassId chooseClass() {
    while (true) {
        std::cout << "\n";
        panelTop("Choose your class", c::accent);
        panelLine(color(c::bad, chip(1)) + bold("Warrior") + dim("  stamina \u00b7 big hits \u00b7 shields"));
        panelLine(color(c::mana, chip(2)) + bold("Mage")    + dim("  mana \u00b7 elemental spells"));
        panelLine(color(c::arcane, chip(3)) + bold("Rogue")   + dim("  energy \u00b7 crits & poison"));
        panelLine(color(c::good, chip(4)) + bold("Paladin")  + dim("  conviction \u00b7 holy power \u00b7 heals"));
        panelLine(color(c::gold, chip(5)) + bold("Necromancer") + dim("  soul \u00b7 rot & bone shields \u00b7 summon"));
        panelBottom();
        const int c = io::askInt("Class", 1, 5);
        switch (c) {
            case 1: return ClassId::Warrior;
            case 2: return ClassId::Mage;
            case 3: return ClassId::Rogue;
            case 4: return ClassId::Paladin;
            case 5: return ClassId::Necromancer;
        }
    }
}

bool chooseMode() {
    while (true) {
        std::cout << "\n";
        panelTop("Choose your fate", c::bad);
        panelLine(chip(1) + bold("Softcore ") + dim("     death is a setback - lose 20% gold, drop a floor, carry on"));
        panelLine(color(c::bad, chip(2)) + color(c::bad, bold("Hardcore ")) + dim("    death is forever - the save is erased, your legend falls"));
        panelLine(dim("  " + std::string(gly(G_BOSS)) + " Hardcore heroes are etched into the Glory track."));
        panelBottom(c::bad);
        const int c = io::askInt("Mode", 1, 2);
        if (c == 2) return true;
        if (c == 1) return false;
    }
}

void choosePerk(Vault& vault) {
    bool anyLeft = false;
    for (int i = 0; i < static_cast<int>(PerkId::kNumPerks); ++i)
        if (!vault.perks[static_cast<std::size_t>(i)]) anyLeft = true;
    if (!anyLeft) {
        std::cout << dim("  Every perk is already woven into your legend.\n");
        return;
    }
    while (true) {
        std::cout << "\n";
        panelTop("Choose a Rift Aspect perk", c::arcane);
        for (int i = 0; i < static_cast<int>(PerkId::kNumPerks); ++i) {
            const auto p = static_cast<PerkId>(i);
            std::string desc;
            switch (p) {
                case PerkId::Heirloom:     desc = "  +20% gold from all sources";       break;
                case PerkId::Runeforge:    desc = "  runestones drop more often";        break;
                case PerkId::Insight:      desc = "  +10% XP gain";                      break;
                case PerkId::Vitals:       desc = "  +8% max HP";                        break;
                case PerkId::Leeching:     desc = "  +2% life steal";                    break;
                case PerkId::Bulwark:      desc = "  +6 defense";                        break;
                case PerkId::Greed:        desc = "  +50% shards & essence from kills";  break;
                case PerkId::Regeneration: desc = "  +2 HP regen each turn";             break;
                case PerkId::Evasion:      desc = "  +8% chance to dodge attacks";       break;
                case PerkId::Bargain:      desc = "  -20% shop and blacksmith prices";   break;
                default: break;
            }
            if (vault.perks[static_cast<std::size_t>(i)])
                panelLine(dim(std::string(gly(G_CK)) + " " + perkName(p) + desc + "  (owned)"));
            else
                panelLine(chip(i + 1) + color(c::gold, perkName(p)) + dim(desc));
        }
        panelLine(dim(chip(0) + "skip"));
        panelBottom(c::arcane);
        const int pick = io::askInt("Perk", 0, static_cast<int>(PerkId::kNumPerks));
        if (pick == 0) return;
        const std::size_t idx = static_cast<std::size_t>(pick - 1);
        if (!vault.perks[idx]) {
            vault.perks[idx] = true;
            panelLine(color(c::arcane, gly(G_GEM)) + " Perk bound: " + perkName(static_cast<PerkId>(pick - 1)));
            std::cout << "\n";
            return;
        }
    }
}

void showCharacter(const Character& pc, const Vault& vault) {
    const auto st = pc.stats(vault);
    const std::string title = std::string(className(pc.classId())) + "  \u00b7  Level "
                            + std::to_string(pc.level()) + "  \u00b7  Floor "
                            + std::to_string(pc.currentFloor());
    std::cout << "\n";
    panelTop(title, c::accent);
    if (pc.hardcore())
        panelLine(color(c::bad, bold("  " + std::string(gly(G_BOSS)) + " HARDCORE \u2014 death is permanent")));
    panelLine("HP " + hpMeter(pc.hp(), st.maxHp, 16)
              + "  " + dim(std::to_string(pc.hp()) + "/" + std::to_string(st.maxHp)));
    panelLine(std::string(resourceName(pc.classId())) + " " + meter(pc.resource(), st.maxResource, 16, c::mana)
              + "  " + dim(std::to_string(pc.resource()) + "/" + std::to_string(st.maxResource)));
    panelLine("XP " + meter(pc.xp(), pc.xpToNext(), 16, c::gold)
              + "  " + dim(std::to_string(pc.xp()) + "/" + std::to_string(pc.xpToNext()))
              + "   " + color(c::accent, std::to_string(pc.skillPoints()) + " skill pt"));
    panelLine("ATK " + bold(std::to_string(st.attack))
              + "   DEF " + bold(std::to_string(st.defense))
              + "   Crit " + std::to_string(st.critChance) + "%"
              + "(+" + std::to_string(st.critBonus) + ")"
              + "   Leech " + std::to_string(st.lifeStealPct) + "%"
              + "   XP+" + std::to_string(st.xpGainPct) + "%");
    std::string pf = "Passive  " + color(c::gold, bold(passiveName(pc.classId())));
    if (vault.mastery > 0)
        pf += "   " + color(c::arcane, std::string(gly(G_GEM)) + " " + std::to_string(vault.mastery) + " Mastery");
    if (vault.aspect > 0)
        pf += "   " + color(c::good, std::string(gly(G_GEM)) + " " + std::to_string(vault.aspect) + " Aspect");
    panelLine(pf);
    panelLine(color(c::gold, "Gold " + std::to_string(vault.gold))
              + "   " + color(c::accent, "Shards " + std::to_string(vault.shards))
              + "   " + color(c::arcane, "Essence " + std::to_string(vault.essence))
              + "   " + color(c::arcane, "Runestones " + std::to_string(vault.runes.size())));
    panelLine(dim("Equipped:"));
    for (int s = 0; s < kNumSlots; ++s) {
        const Slot slot = static_cast<Slot>(s);
        const int idx = vault.equippedIndex(slot);
        std::string line = "   " + color(c::soft, std::string(slotName(slot)) + ": ");
        if (vault.hasItem(idx))
            line += std::string(itemIcon(vault.items[static_cast<std::size_t>(idx)])) + " "
                  + color(rarityColor(vault.items[static_cast<std::size_t>(idx)].rarity),
                          vault.items[static_cast<std::size_t>(idx)].name);
        else
            line += dim("(empty)");
        panelLine(line);
    }
    panelBottom();
    std::cout << "\n";
}

void showVault(const Vault& vault, const std::vector<int>& gear) {
    if (gear.empty()) {
        panelLine(dim("No gear to manage."));
        return;
    }
    for (std::size_t k = 0; k < gear.size(); ++k) {
        const Item& it = vault.items[static_cast<std::size_t>(gear[k])];
        const int eq = vault.equippedIndex(it.slot);
        std::string tag;
        if (eq == gear[k]) tag = color(c::good, " [equipped]");
        else if (eq >= 0)  tag = dim(" (slot: " + vault.items[static_cast<std::size_t>(eq)].name + ")");
        panelLine(chip(static_cast<int>(k + 1)) + itemIcon(it) + " "
                  + color(rarityColor(it.rarity), it.describe()) + tag);
    }
}

void equipMenu(Character& pc, Vault& vault) {
    (void)pc;
    while (true) {
        std::cout << "\n";
        panelTop("Manage gear", c::accent);
        panelLine(chip(1) + color(c::neutral, std::string(gly(G_SWORD)) + " ") + "Weapons");
        panelLine(chip(2) + color(c::accent, std::string(gly(G_SHIELD)) + " ") + "Defenses  (armor, helm, gloves, boots)");
        panelLine(chip(3) + color(c::arcane, std::string(gly(G_RING)) + " ") + "Trinkets  (rings, amulets, relics)");
        panelLine(dim(chip(0) + "back"));
        panelBottom();
        const int g = io::askInt("Group", 0, 3);
        if (g == 0) return;
        std::vector<int> gear;
        for (std::size_t i = 0; i < vault.items.size(); ++i) {
            const Item& it = vault.items[i];
            if (!it.isPot() && slotInGroup(it.slot, g))
                gear.push_back(static_cast<int>(i));
        }
        if (gear.empty()) {
            std::cout << dim("  No gear in that group.\n");
            continue;
        }
        while (true) {
            std::cout << "\n";
            panelTop("Gear", c::accent);
            showVault(vault, gear);
            panelLine(dim(chip(0) + "back"));
            panelBottom();
            const int pick = io::askInt("Equip / unequip", 0, static_cast<int>(gear.size()));
            if (pick == 0) break;
            const int idx = gear[static_cast<std::size_t>(pick - 1)];
            const Item& it = vault.items[static_cast<std::size_t>(idx)];
            if (vault.equippedIndex(it.slot) == idx) {
                vault.unequip(it.slot);
                panelLine(color(c::frost, gly(G_DI)) + " Unequipped "
                          + color(rarityColor(it.rarity), it.name));
            } else {
                vault.equip(idx);
                panelLine(color(c::good, gly(G_DI)) + " Equipped "
                          + color(rarityColor(it.rarity), it.name));
            }
            std::cout << "\n";
        }
    }
}

void spellBook(const Character& pc, const Vault& vault) {
    const auto& spells = classSpells(pc.classId());
    const auto tree = pc.tree();
    const int atk = pc.stats(vault).attack;
    std::cout << "\n";
    panelTop(std::string("Spell tree \u2014 ") + std::to_string(pc.skillPoints()) + " points", c::mana);
    for (int b = 0; b < 3; ++b) {
        panelLine(color(c::gold, bold(branchName(pc.classId(), b))));
        for (int i = b * 4; i < b * 4 + 4; ++i) {
            const Spell& s = spells[static_cast<std::size_t>(i)];
            const bool learned = tree[static_cast<std::size_t>(i)];
            const std::string codeStr = std::to_string((i % 4) + 1) + " "
                                      + std::to_string(b + 1);
            if (learned) {
                const std::string glyStr = gly(G_MED);
                const int headLen = 3 + static_cast<int>(codeStr.size()) + 1
                                    + static_cast<int>(glyStr.size()) + 1
                                    + static_cast<int>(s.name.size());
                const int budget = std::max(layoutWidth() - 2 - headLen, 4);
                const std::string head = "   " + color(c::accent, codeStr) + " "
                                       + color(c::gold, glyStr + " " + s.name);
                const std::string wrapped = wrap(" \u2014 "
                                                 + spellBlurb(s, pc.level(), pc.currentFloor(), atk),
                                                 budget);
                const std::string pad(std::size_t(headLen), ' ');
                std::size_t pos = 0;
                while (true) {
                    const std::size_t nl = wrapped.find('\n', pos);
                    const std::string seg = wrapped.substr(pos, nl == std::string::npos
                                           ? std::string::npos : nl - pos);
                    panelLine((pos == 0 ? head : color(c::soft, pad))
                              + color(c::soft, seg));
                    if (nl == std::string::npos) break;
                    pos = nl + 1;
                }
            } else {
                const bool trainable = (i % 4 == 0) ||
                                       tree[static_cast<std::size_t>(i - 1)];
                panelLine(dim(std::string(gly(G_SK)) + " ")
                          + color(c::accent, codeStr)
                          + dim(" " + s.name)
                          + " " + color(c::soft, "[" + std::to_string((i % 4) + 1) + "pt]")
                          + (trainable ? std::string() : dim("  locked")));
            }
        }
    }
    panelBottom(c::mana);
    std::cout << "\n";
}

bool passiveTrain(Character& pc) {
    while (true) {
        std::cout << "\n";
        panelTop("Passive training", c::accent);
        panelWrapped("Spend 1 skill point per rank (max "
                     + std::to_string(kTrainMaxRank) + " ranks each) — permanent.",
                     layoutWidth() - 4, " ");
        for (int i = 0; i < kNumTrains; ++i) {
            const auto t = static_cast<TrainId>(i);
            const int rank = pc.trainRank(t);
            std::string rankStr = rank >= kTrainMaxRank
                                  ? color(c::gold, "MAX")
                                  : color(c::soft, "rank " + std::to_string(rank) + "/"
                                        + std::to_string(kTrainMaxRank));
            const std::string wrapped = wrap("  " + std::string(trainDesc(t)),
                                             layoutWidth() - 12);
            std::size_t pos = 0;
            bool firstSeg = true;
            while (true) {
                const std::size_t nl = wrapped.find('\n', pos);
                const std::string seg = wrapped.substr(pos, nl == std::string::npos
                                       ? std::string::npos : nl - pos);
                if (firstSeg) {
                    panelLine(chip(i + 1) + color(c::gold, trainName(t))
                              + dim(seg) + dim("   ") + rankStr);
                    firstSeg = false;
                } else {
                    panelLine(dim("        ") + dim(seg));
                }
                if (nl == std::string::npos) break;
                pos = nl + 1;
            }
        }
        panelLine(dim(chip(0) + "back"));
        panelBottom();
        const int pick = io::askInt("Train", 0, kNumTrains);
        if (pick == 0) return false;
        const auto t = static_cast<TrainId>(pick - 1);
        if (pc.train(t)) {
            panelLine(color(c::good, gly(G_DI)) + " " + trainName(t)
                      + " rank " + std::to_string(pc.trainRank(t)));
            std::cout << "\n";
        } else {
            std::cout << dim("  No points left, or already maxed.\n");
        }
    }
}

bool spellTrainer(Character& pc, const Vault& vault) {
    while (true) {
        spellBook(pc, vault);
        std::cout << "  " << dim("Learn: ") << color(c::accent, "'<rank> <branch>'") << dim(", e.g. ")
                  << color(c::accent, "1 1") << dim("   ") << color(c::accent, "<P>") << dim(" passives")
                  << dim("   ") << color(c::accent, "<0>") << dim(" exit\n");
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        if (line.empty()) continue;
        if (line[0] == '0') return false;
        if (line[0] == 'p' || line[0] == 'P') { passiveTrain(pc); continue; }
        int r = 0, b = 0;
        if (std::sscanf(line.c_str(), "%d %d", &r, &b) != 2) continue;
        if (r < 1 || r > 4 || b < 1 || b > 3) continue;
        const Spell& s = pc.spell(b - 1, r - 1);
        if (pc.spendPoint(b - 1, r - 1)) {
            panelLine(color(c::mana, gly(G_DI)) + " Learned " + s.name
                      + color(c::soft, "  \u2014 " + spellBlurb(s, pc.level(), pc.currentFloor(),
                                                             pc.stats(vault).attack)));
            std::cout << "\n";
        } else {
            std::cout << dim("  Can't learn that (prerequisites or points).\n");
        }
    }
}

void recordsScreen(const Character& pc, const Vault& vault) {
    (void)pc;
    const auto& v = vault;
    std::cout << "\n";
    panelTop("Records & Achievements", c::arcane);
    panelLine(color(c::accent, "Best floor " + std::to_string(v.bestFloor))
              + "   " + color(c::bad, "Bosses " + std::to_string(v.bossesSlain))
              + "   " + color(c::neutral, "Kills " + std::to_string(v.kills))
              + "   " + color(c::soft, "Deaths " + std::to_string(v.deaths)));
    panelLine(color(c::gold, "Legendaries " + std::to_string(v.legendaryFound))
              + "   " + color(c::arcane, "Mastery " + std::to_string(v.mastery))
              + "   " + color(c::good, "Ascensions " + std::to_string(v.aspect)));
    panelLine(dim(""));

    const auto gs = rpg::glory::load();
    panelLine(color(c::arcane, bold("Glory ")) +
              std::to_string(rpg::glory::unlockedCount(gs)) + "/"
              + std::to_string(rpg::glory::kNumAchievements));
    for (int i = 0; i < rpg::glory::kNumAchievements; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        panelLine(gs.unlocked[idx]
                      ? color(c::gold, std::string(gly(G_CK)) + " " + rpg::glory::achievementName(i))
                      : dim(std::string(gly(G_SK)) + " " + rpg::glory::achievementName(i)));
    }
    if (!gs.fallen.empty()) {
        panelLine(dim(""));
        panelLine(color(c::bad, bold(std::string(gly(G_BOSS)) + " Fallen heroes")));
        for (const auto& f : gs.fallen)
            panelLine(dim("  " + std::string(className(f.cls)) + "  L"
                          + std::to_string(f.level) + "  \u00b7  Floor "
                          + std::to_string(f.floor) + "  \u00b7  "
                          + std::to_string(f.kills) + " kills"));
    }

    panelLine(dim(""));
    panelLine(color(c::accent, bold("Codex")));
    if (v.bestiary.totalKills() > 0) {
        for (const auto& kv : v.bestiary.enemies)
            panelLine(dim("  " + kv.first) + color(c::soft, " x" + std::to_string(kv.second)));
    } else {
        panelLine(dim("  No creatures recorded yet."));
    }
    if (v.bestiary.totalBosses() > 0) {
        panelLine(dim(""));
        panelLine(color(c::bad, bold("Bosses")));
        for (const auto& kv : v.bestiary.bosses)
            panelLine(dim("  " + kv.first) + color(c::soft, " x" + std::to_string(kv.second)));
    }
    if (!v.bestiary.biomes.empty()) {
        panelLine(dim(""));
        panelLine(color(c::gold, bold("Biomes seen")));
        for (const auto& kv : v.bestiary.biomes)
            panelLine(dim("  " + kv.first) + color(c::soft, " x" + std::to_string(kv.second)));
    }
    if (!v.bestiary.affixes.empty()) {
        panelLine(dim(""));
        panelLine(color(c::arcane, bold("Affixes witnessed")));
        for (const auto& kv : v.bestiary.affixes)
            panelLine(dim("  " + kv.first) + color(c::soft, " x" + std::to_string(kv.second)));
    }
    panelBottom(c::arcane);
    std::cout << dim("  (press enter to return)\n");
    std::string line;
    if (!std::getline(std::cin, line)) return;
}

void floorEvent(Character& pc, Vault& vault, int floor, core::Rng& rng) {
    const auto st = pc.stats(vault);
    std::cout << "\n";
    const int kind = static_cast<int>(rng.pick(14));
    switch (kind) {
        case 0: {
            panelTop("A Glimmering Fountain", c::gold);
            const int h = std::max(1, st.maxHp / 3);
            pc.healHp(h, st.maxHp);
            pc.restoreResource(st.maxResource / 2, st.maxResource);
            panelLine(color(c::gold, "  Cool waters mend your wounds.  +" + std::to_string(h) + " HP."));
            panelBottom(c::gold);
            break;
        }
        case 1: {
            panelTop("A Forgotten Cache", c::gold);
            const int base = 10 * floor + rng.roll(5, 30);
            const int g = base + base * st.goldGainPct / 100;
            vault.gold += g;
            vault.totalGoldEarned += g;
            panelLine(color(c::gold, "  You pry the cache open:  " + std::to_string(g) + "  gold."));
            panelBottom(c::gold);
            break;
        }
        case 2: {
            panelTop("A Rune-Fall", c::arcane);
            const int n = rng.chance(0.5) ? 1 : 2;
            for (int i = 0; i < n; ++i) vault.runes.push_back(makeRune(floor, rng));
            panelLine(color(c::arcane, "  Raw runestone dust crystallizes:  +" + std::to_string(n)
                              + std::string(n == 1 ? " rune." : " runes.")));
            panelBottom(c::arcane);
            break;
        }
        case 3: {
            panelTop("A Hidden Trap!", c::bad);
            if (rng.chance(0.5)) {
                panelLine(color(c::good, "  Spike plates snap shut \u2014 but you dive clear!"));
            } else {
                const int d = std::max(1, st.maxHp / 4);
                pc.takeDamage(d);
                panelLine(color(c::bad, "  Spike plates snap shut for "
                                  + std::to_string(d) + " damage."));
            }
            panelBottom(c::bad);
            break;
        }
        case 4: {
            panelTop("A Traveling Apothecary", c::mana);
            const int brewPrice = discountedCost(40, st.vendorDiscountPct);
            panelLine(color(c::accent, "  She offers a freshly brewed Healing Draught."));
            panelLine(chip(1) + " accept (" + std::to_string(brewPrice) + "g)      "
                      + dim(chip(0) + " decline"));
            panelBottom(c::mana);
            const int pick = io::askInt("Choose", 0, 1);
            if (pick == 1 && vault.gold >= brewPrice) {
                vault.gold -= brewPrice;
                vault.addPotion(makeVendorPotion(floor, false));
                panelLine(color(c::good, gly(G_DI)) + " You buy the draught.");
                std::cout << "\n";
            } else if (pick == 1) {
                panelLine(color(c::bad, "  Not enough gold; she waves you on."));
                std::cout << "\n";
            }
            break;
        }
        case 5: {
            panelTop("A Fading Shrine", c::arcane);
            vault.runes.push_back(makeRune(floor, rng));
            pc.healHp(std::max(1, st.maxHp / 5), st.maxHp);
            panelLine(color(c::arcane, "  The shrine answers: a runestone forms in your palm."));
            panelLine(color(c::good, "  Warmth settles over you.  +" + std::to_string(std::max(1, st.maxHp / 5)) + " HP."));
            panelBottom(c::arcane);
            break;
        }
        case 6: {
            panelTop("A Hexed Chest", c::bad);
            if (rng.chance(0.5)) {
                const int d = std::max(1, st.maxHp / 5);
                pc.takeDamage(d);
                const int lost = std::min(vault.gold, 10 + floor);
                vault.gold -= lost;
                panelLine(color(c::bad, "  The chest bites back: " + std::to_string(d) + " dmg, "
                                  + std::to_string(lost) + " gold vanishes."));
            } else {
                vault.runes.push_back(makeRune(floor, rng));
                const int base = rng.roll(4 * floor, 8 * floor);
                const int g = base + base * st.goldGainPct / 100;
                vault.gold += g;
                vault.totalGoldEarned += g;
                panelLine(color(c::gold, "  A trap pulls aside — "+ std::to_string(g) + " gold and a runestone wait inside."));
            }
            panelBottom(c::bad);
            break;
        }
        case 7: {
            panelTop("A Whispering Mural", c::gold);
            const int xp = 25 + 15 * floor;
            pc.gainXp(xp, st.xpGainPct);
            const int base = 20 + 5 * floor;
            const int g = base + base * st.goldGainPct / 100;
            vault.gold += g;
            vault.totalGoldEarned += g;
            panelLine(color(c::gold, "  Runes on the wall teach you the floor's secrets.  +" + std::to_string(xp) + " XP."));
            panelBottom(c::gold);
            break;
        }
        case 8: {
            panelTop("A Storm of Runes", c::arcane);
            for (int i = 0; i < 2; ++i) vault.runes.push_back(makeRune(floor, rng));
            pc.restoreResource(st.maxResource / 2, st.maxResource);
            panelLine(color(c::arcane, "  Runic lightning arcs between stones:  +2 runes."));
            panelBottom(c::arcane);
            break;
        }
        case 9: {
            panelTop("A Blessing Altar", c::arcane);
            panelLine(color(c::accent, "  Two runes rest on the altar \u2014 one warm, one cold."));
            panelLine(chip(1) + " warm rune: mend wounds      " + chip(2) + " cold rune: claim a runestone");
            panelBottom(c::arcane);
            const int pick = io::askInt("Choose", 1, 2);
            if (pick == 1) {
                const int h = std::max(1, st.maxHp / 2);
                pc.healHp(h, st.maxHp);
                panelLine(color(c::good, "  Warm light knits your flesh.  +" + std::to_string(h) + " HP."));
            } else {
                vault.runes.push_back(makeRune(floor, rng));
                panelLine(color(c::arcane, "  The cold rune crystallizes in your hand.  +1 rune."));
            }
            std::cout << "\n";
            break;
        }
        case 10: {
            panelTop("A Wandering Demon", c::bad);
            panelLine(color(c::bad, "  It grins and rattles a cup of dice."));
            if (vault.gold < 40) {
                panelLine(dim("  You lack the 40 gold it demands. It slinks away."));
                panelBottom(c::bad);
                break;
            }
            panelLine(chip(1) + " wager 40 gold      " + dim(chip(0) + " refuse"));
            panelBottom(c::bad);
            const int pick = io::askInt("Choose", 0, 1);
            if (pick != 1) {
                panelLine(dim("  You wave it off; it vanishes in a puff of brimstone."));
                std::cout << "\n";
                break;
            }
            vault.gold -= 40;
            const int roll = static_cast<int>(rng.pick(4));
            if (roll == 0) {
                vault.addPotion(makeVendorPotion(floor, false));
                panelLine(color(c::good, "  The dice come up a vial:  +1 Healing Draught."));
            } else if (roll == 1) {
                vault.runes.push_back(makeRune(floor, rng));
                panelLine(color(c::arcane, "  The dice come up a stone:  +1 rune."));
            } else if (roll == 2) {
                const int g = rng.roll(4 * floor, 8 * floor);
                vault.gold += g;
                vault.totalGoldEarned += g;
                panelLine(color(c::gold, "  The dice come up coins:  +" + std::to_string(g) + " gold."));
            } else {
                panelLine(color(c::bad, "  The dice come up empty.  It cackles and is gone."));
            }
            std::cout << "\n";
            break;
        }
        case 11: {
            panelTop("An Abandoned Campfire", c::gold);
            panelLine(color(c::accent, "  Embers still glow beneath the ash."));
            panelLine(chip(1) + " rest: full heal      " + chip(2) + " burn a runestone for XP");
            panelBottom(c::gold);
            const int pick = io::askInt("Choose", 1, 2);
            if (pick == 1) {
                const int before = pc.hp();
                pc.healHp(st.maxHp, st.maxHp);
                panelLine(color(c::good, "  You rest by the fire.  +" + std::to_string(pc.hp() - before) + " HP."));
            } else if (!vault.runes.empty()) {
                vault.runes.pop_back();
                const int xp = 40 + 20 * floor;
                const int gained = xp + xp * st.xpGainPct / 100;
                pc.gainXp(xp, st.xpGainPct);
                panelLine(color(c::gold, "  You feed a runestone to the flame:  +" + std::to_string(gained) + " XP."));
            } else {
                panelLine(dim("  You have no runestone to burn."));
            }
            std::cout << "\n";
            break;
        }
        case 12: {
            panelTop("Echo of a Past Hero", c::mana);
            panelLine(color(c::accent, "  A shade offers the relics it can no longer use."));
            Item rare = makeGear(floor, rng, Rarity::Rare);
            Item epic = makeGear(floor, rng, Rarity::Epic);
            panelLine(chip(1) + " " + color(rarityColor(rare.rarity), rare.describe()));
            panelLine(chip(2) + " " + color(rarityColor(epic.rarity), epic.describe()));
            panelLine(chip(3) + " 2 runestones");
            panelBottom(c::mana);
            const int pick = io::askInt("Choose", 1, 3);
            if (pick == 1) {
                vault.add(rare);
                panelLine(color(c::good, gly(G_DI)) + " You take the "
                          + color(rarityColor(rare.rarity), rare.name) + ".");
            } else if (pick == 2) {
                vault.add(epic);
                panelLine(color(c::good, gly(G_DI)) + " You take the "
                          + color(rarityColor(epic.rarity), epic.name) + ".");
            } else {
                vault.runes.push_back(makeRune(floor, rng));
                vault.runes.push_back(makeRune(floor, rng));
                panelLine(color(c::arcane, "  You take two runestones."));
            }
            std::cout << "\n";
            break;
        }
        default: {
            panelTop("A Puzzling Obelisk", c::accent);
            panelLine(color(c::accent, "  Two runes glow — one grants fortune, one brings misfortune."));
            panelLine(chip(1) + " touch the left rune      " + chip(2) + " touch the right rune");
            panelLine(dim(chip(0) + " walk away"));
            panelBottom(c::accent);
            const int pick = io::askInt("Choose", 0, 2);
            if (pick == 1 || pick == 2) {
                if (rng.chance(0.5)) {
                    vault.shards += rng.roll(3, 12);
                    pc.healHp(std::max(1, st.maxHp / 4), st.maxHp);
                    panelLine(color(c::good, "  The obelisk hums approval: shards and vitality flow to you."));
                } else {
                    const int d = std::max(1, st.maxHp / 6);
                    pc.takeDamage(d);
                    panelLine(color(c::bad, "  The rune flares cold: " + std::to_string(d) + " damage."));
                }
                std::cout << "\n";
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// camp
// ---------------------------------------------------------------------------

bool blacksmith(Character& pc, Vault& vault, core::Rng& rng) {
    while (true) {
        std::cout << "\n";
        panelTop("Blacksmith", c::soft);
        const int disc = pc.stats(vault).vendorDiscountPct;
        panelLine("  " + color(c::gold, "Gold ") + std::to_string(vault.gold)
                  + color(c::accent, "   Shards ") + std::to_string(vault.shards)
                  + color(c::arcane, "   Essence ") + std::to_string(vault.essence));
        std::vector<int> gear;
        for (std::size_t i = 0; i < vault.items.size(); ++i)
            if (!vault.items[i].isPot()) gear.push_back(static_cast<int>(i));
        if (gear.empty()) {
            panelLine(dim("No gear to forge."));
            panelBottom();
            return false;
        }
        for (std::size_t k = 0; k < gear.size(); ++k) {
            const Item& it = vault.items[static_cast<std::size_t>(gear[k])];
            std::string tag = equippedItem(vault, gear[k])
                              ? color(c::good, " [equipped]") : std::string();
            std::string hints;
            hints += color(c::neutral, "  [1] upgrade " + shards(discountedCost(upgradeCost(it), disc)));
            hints += color(c::frost, "  [2] reforge " + gold(discountedCost(reforgeCost(it), disc)));
            if (canAwaken(it)) hints += color(c::arcane, "  [3] awaken " + std::to_string(discountedCost(awakenCost(it), disc)) + " es");
            if (it.freeSockets() > 0) hints += color(c::accent, "  [4] socket");
            if (!it.runes.empty())   hints += color(c::flare, "  [5] extract " + gold(discountedCost(runeExtractCost(it), disc)));
            panelLine(chip(static_cast<int>(k + 1))
                      + itemIcon(it) + " "
                      + color(rarityColor(it.rarity), it.describe()) + tag);
            panelLine(color(c::soft, "        ") + hints);
        }
        panelLine(dim(chip(0) + "back"));
        panelBottom();
        const int pick = io::askInt("Item", 0, static_cast<int>(gear.size()));
        if (pick == 0) return false;
        const int idx = gear[static_cast<std::size_t>(pick - 1)];
        Item& it = vault.items[static_cast<std::size_t>(idx)];
        const int act = io::askInt("Action (0 back)", 0, 5);
        if (act == 0) continue;
        if (act == 1) {
            if (vault.shards < discountedCost(upgradeCost(it), disc))
                std::cout << dim("  Not enough shards.\n");
            else if (!canUpgrade(it, pc.currentFloor()))
                std::cout << dim("  Item already at the floor cap.\n");
            else {
                vault.shards -= discountedCost(upgradeCost(it), disc);
                upgrade(it);
                panelLine(color(c::neutral, gly(G_SWORD)) + " Forged to iLvl " + std::to_string(it.iLvl)
                          + color(rarityColor(it.rarity), "  " + it.describe()));
                std::cout << "\n";
            }
        } else if (act == 2) {
            if (vault.gold < discountedCost(reforgeCost(it), disc))
                std::cout << dim("  Not enough gold.\n");
            else {
                vault.gold -= discountedCost(reforgeCost(it), disc);
                reforge(it, rng);
                panelLine(color(c::frost, gly(G_DI)) + " Rerolled " + color(rarityColor(it.rarity), it.describe()));
                std::cout << "\n";
            }
        } else if (act == 3) {
            if (!canAwaken(it))
                std::cout << dim("  Can't awaken that one.\n");
            else if (vault.essence < discountedCost(awakenCost(it), disc))
                std::cout << dim("  Not enough essence.\n");
            else {
                vault.essence -= discountedCost(awakenCost(it), disc);
                awaken(it, rng);
                panelLine(color(c::arcane, std::string(gly(G_GEM)) + " Awakened ") + color(rarityColor(it.rarity), it.describe()));
                std::cout << "\n";
            }
        } else if (act == 4) {
            socketAction(it, vault);
        } else if (act == 5) {
            extractAction(it, vault, disc);
        }
    }
}

bool merchantMenu(Character& pc, Vault& vault, core::Rng& rng) {
    const int floor = pc.currentFloor();
    const int disc = pc.stats(vault).vendorDiscountPct;
    const int hpPrice = discountedCost(8 + 2 * floor, disc);
    const int mpPrice = discountedCost(6 + 2 * floor, disc);
    const int shardPrice = discountedCost(12, disc);
    const int essencePrice = discountedCost(30, disc);
    const int setPrice = discountedCost(150 + 40 * floor, disc);
    SetId stock = rollSet(floor, rng);   // curated set wares; None = nothing today
    while (true) {
        std::cout << "\n";
        panelTop("Merchant", c::gold);
        panelLine("  " + color(c::gold, "Gold ") + std::to_string(vault.gold)
                  + color(c::accent, "   Shards ") + std::to_string(vault.shards)
                  + color(c::arcane, "   Essence ") + std::to_string(vault.essence));
        panelLine(chip(1) + std::string(gly(G_POTION)) + " "
                  + color(c::bad, "Healing Draught") + "  " + gold(hpPrice));
        panelLine(chip(2) + std::string(gly(G_POTION)) + " "
                  + color(c::frost, "Mana Draught") + "  " + gold(mpPrice));
        panelLine(chip(3) + std::string(gly(G_SHARD)) + " "
                  + color(c::accent, "Shards \u00d73") + "  " + gold(shardPrice));
        panelLine(chip(4) + std::string(gly(G_ESSENCE)) + " "
                  + color(c::arcane, "Essence") + "  " + gold(essencePrice));
        panelLine(chip(5) + std::string(gly(G_GOLD)) + " " + dim("Sell gear"));
        panelLine(chip(6) + std::string(gly(G_ESSENCE)) + " " + dim("Salvage gear"));
        if (stock != SetId::None)
            panelLine(chip(7) + std::string(gly(G_SWORD)) + " "
                      + color(c::arcane, std::string(setName(stock)) + " set piece") + "  " + gold(setPrice));
        panelLine(dim(chip(0) + "back"));
        panelBottom(c::gold);
        const int maxOpt = stock != SetId::None ? 7 : 6;
        const int c = io::askInt("Buy", 0, maxOpt);
        if (c == 0) return false;
        if (c == 5) { sellMenu(vault); continue; }
        if (c == 6) { salvageMenu(vault); continue; }
        if (c == 7) {
            if (stock == SetId::None) continue;
            if (vault.gold < setPrice) {
                std::cout << dim("  Not enough gold.\n");
                continue;
            }
            vault.gold -= setPrice;
            Item piece = makeGear(floor, rng, Rarity::Rare);
            piece.setTag = stock;
            vault.add(piece);
            panelLine(color(c::arcane, gly(G_DI)) + " Bought "
                      + color(rarityColor(piece.rarity), piece.describe()));
            std::cout << "\n";
            stock = SetId::None;
            continue;
        }
        Item buy;
        int price = 0;
        if (c == 1) { price = hpPrice; buy = makeVendorPotion(floor + rng.roll(0, 2), false); }
        else if (c == 2) { price = mpPrice; buy = makeVendorPotion(floor + rng.roll(0, 2), true); }
        else if (c == 3) { price = shardPrice; }
        else if (c == 4) { price = essencePrice; }
        if (price > 0) {
            if (vault.gold < price) {
                std::cout << dim("  Not enough gold.\n");
                continue;
            }
            vault.gold -= price;
            if (c == 3) {
                vault.shards += 3;
                panelLine(color(c::good, gly(G_DI)) + " Purchased");
            } else if (c == 4) {
                vault.essence += 1;
                panelLine(color(c::good, gly(G_DI)) + " Purchased");
            } else {
                vault.addPotion(buy);
                panelLine(color(c::gold, gly(G_DI)) + " Bought " + buy.describe(pc.currentFloor()));
            }
            std::cout << "\n";
        }
    }
}

void inventoryMenu(const Character& pc, Vault& vault) {
    while (true) {
        std::cout << "\n";
        panelTop(std::string("Inventory \u2014 Floor ") + std::to_string(pc.currentFloor()), c::accent);
        panelLine("  " + color(c::gold, "Gold ") + std::to_string(vault.gold)
                  + color(c::accent, "   Shards ") + std::to_string(vault.shards)
                  + color(c::arcane, "   Essence ") + std::to_string(vault.essence));
        bool anyGear = false;
        for (int g = 1; g <= 3; ++g) {
            const char* gn = g == 1 ? "Weapons" : g == 2 ? "Defenses" : "Trinkets";
            std::vector<int> gear;
            for (std::size_t i = 0; i < vault.items.size(); ++i) {
                const Item& it = vault.items[i];
                if (!it.isPot() && slotInGroup(it.slot, g)) gear.push_back(static_cast<int>(i));
            }
            if (gear.empty()) continue;
            anyGear = true;
            panelLine(color(c::neutral, bold(std::to_string(g)) + "  " + bold(gn)));
            for (const int i : gear) {
                const Item& it = vault.items[static_cast<std::size_t>(i)];
                std::string tag = equippedItem(vault, i)
                                  ? color(c::good, " [equipped]") : std::string();
                panelLine(std::string("   ") + itemIcon(it) + " "
                          + color(rarityColor(it.rarity), it.describe()) + tag);
            }
        }
        if (!anyGear) panelLine(dim("  No gear carried."));
        panelLine(dim(""));
        bool anyPot = false;
        for (const auto& it : vault.items) {
            if (!it.isPot()) continue;
            anyPot = true;
            panelLine(std::string("   ") + itemIcon(it) + " "
                      + color(rarityColor(it.rarity), it.describe(pc.currentFloor())));
        }
        if (!anyPot) panelLine(dim("  No potions."));
        if (!vault.runes.empty()) {
            panelLine(dim(""));
            panelLine(color(c::arcane, std::string(gly(G_RUNE)) + "  Runestones: "
                            + std::to_string(vault.runes.size())));
        }
        panelLine(dim(chip(0) + "back"));
        panelBottom();
        if (io::askInt("Back", 0, 0) == 0) return;
    }
}

void beltMenu(Vault& vault) {
    const char* kindNames[2] = { "Healing Draught", "Mana Draught" };
    while (true) {
        std::cout << "\n";
        panelTop("Belt quick-slots", c::accent);
        panelLine(color(c::soft, " Belt quicks; press 1/2 in combat to drink instantly."));
        for (int s = 0; s < 2; ++s) {
            const PotionKind k = vault.beltKind(s);
            std::string line = "  Belt " + std::to_string(s + 1) + ": ";
            if (k == PotionKind::None) {
                line += dim("(empty)");
            } else {
                const int n = potionCount(vault, k);
                line += std::string(gly(G_POTION)) + " " + kindNames[static_cast<int>(k) - 1]
                      + (n > 0 ? color(c::good, "  x" + std::to_string(n))
                               : dim("  (none carried)"));
            }
            panelLine(line);
        }
        panelLine(dim(""));
        panelLine(chip(1) + dim("bind slot 1") + "   " + chip(2) + dim("bind slot 2"));
        panelLine(chip(3) + dim("clear slot 1") + "  " + chip(4) + dim("clear slot 2"));
        panelLine(dim(chip(0) + "back"));
        panelBottom();
        const int pick = io::askInt("Action", 0, 4);
        if (pick == 0) return;
        if (pick == 3) { vault.bindBelt(0, PotionKind::None); continue; }
        if (pick == 4) { vault.bindBelt(1, PotionKind::None); continue; }
        panelTop("Bind belt slot " + std::to_string(pick), c::accent);
        panelLine(chip(1) + std::string(gly(G_POTION)) + " "
                  + color(c::good, "Healing Draught"));
        panelLine(chip(2) + std::string(gly(G_POTION)) + " "
                  + color(c::frost, "Mana Draught"));
        panelLine(dim(chip(0) + "cancel"));
        panelBottom();
        const int bp = io::askInt("Bind which kind? (0 back)", 0, 2);
        if (bp == 0) continue;
        vault.bindBelt(pick - 1, bp == 1 ? PotionKind::Healing : PotionKind::Mana);
        panelLine(color(c::good, gly(G_DI)) + " Bound belt slot " + std::to_string(pick)
                  + " to " + kindNames[bp - 1]);
        std::cout << "\n";
    }
}

bool respecMenu(Character& pc, Vault& vault) {
    const int cost = 25 + 15 * pc.level();
    std::cout << "  Respec costs " << gold(cost)
              << ". Points refunded: " << pc.pointsSpent() << "\n";
    if (!yesNo("Respec?")) return false;
    if (vault.gold < cost) {
        std::cout << dim("  Not enough gold.\n");
        return false;
    }
    vault.gold -= cost;
    pc.respec();
    panelLine(color(c::accent, gly(G_DI)) + " Tree cleared. " + std::to_string(pc.skillPoints()) + " points refunded");
    std::cout << "\n";
    return true;
}

CampResult camp(Character& pc, Vault& vault, core::Rng& rng) {
    while (true) {
        showCharacter(pc, vault);
        panelTop(std::string("Camp \u2014 Floor ") + std::to_string(pc.currentFloor()), c::soft);
        panelLine(chip(1) + std::string(gly(G_BED)) + " " + bold("Rest ") + dim("      full recovery"));
        panelLine(chip(2) + std::string(gly(G_ANVIL)) + " " + bold("Blacksmith ") + dim("  forge · reroll · sockets"));
        panelLine(chip(3) + std::string(gly(G_GOLD)) + " " + bold("Merchant ") + dim("    buy, sell & salvage"));
        panelLine(chip(4) + std::string(gly(G_SWORD)) + " " + bold("Equip gear"));
        panelLine(chip(5) + std::string(gly(G_POTION)) + " " + bold("Belt ") + dim("        bind quick potions (1/2 in combat)"));
        panelLine(chip(6) + std::string(gly(G_TOME)) + " " + bold("Trainer ") + dim("    spells · passives"));
        panelLine(chip(7) + std::string(gly(G_SK)) + " " + bold("Respec ") + dim("      refund the spell tree"));
        panelLine(chip(8) + std::string(gly(G_CHEST)) + " " + bold("Inventory ") + dim("  browse gear & potions"));
        panelLine(color(c::gold, chip(9) + std::string(gly(G_STAIRS)) + " " + bold("Descend ") + dim("      head deeper to Floor ")
                  + std::to_string(pc.currentFloor() + 1)));
        panelLine(chip(10) + std::string(gly(G_NOTE)) + " " + bold("Records ") + dim("    & achievements"));
        if (pc.currentFloor() >= 100)
            panelLine(color(c::arcane, chip(11) + std::string(gly(G_GEM)) + " " + bold("Ascend the Rift ") + dim(" prestige +1")));
        panelLine(dim(chip(0) + "Save & quit"));
        panelBottom();
        const int hi = pc.currentFloor() >= 100 ? 11 : 10;
        const int c = io::askInt("Camp", 0, hi);
        switch (c) {
            case 1: {
                const auto st = pc.stats(vault);
                pc.restoreAll(st.maxHp, st.maxResource);
                panelLine(color(c::good, gly(G_DI)) + " You rest and recover completely.");
                std::cout << "\n";
                break;
            }
            case 2: blacksmith(pc, vault, rng); break;
            case 3: merchantMenu(pc, vault, rng); break;
            case 4: equipMenu(pc, vault); break;
            case 5: beltMenu(vault); break;
            case 6: spellTrainer(pc, vault); break;
            case 7: respecMenu(pc, vault); break;
            case 8: inventoryMenu(pc, vault); break;
            case 9: {
                pc.setFloor(pc.currentFloor() + 1);
                const auto st = pc.stats(vault);
                pc.restoreAll(st.maxHp, st.maxResource);
                panelLine(color(c::gold, std::string(gly(G_STAIRS))) + " You descend to Floor "
                          + std::to_string(pc.currentFloor()) + "...");
                std::cout << "\n";
                return CampResult::Descend;
            }
            case 10: recordsScreen(pc, vault); break;
            case 11:
                if (pc.currentFloor() >= 100) return CampResult::Ascend;
                break;
            default: return CampResult::Quit;
        }
    }
}

} // namespace rpg::ui