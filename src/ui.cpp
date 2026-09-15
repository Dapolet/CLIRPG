#include "ui.hpp"

#include "io.hpp"
#include "save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <sys/ioctl.h>
#include <unistd.h>

namespace rpg::ui {

namespace {

// Plain mode: color helpers become identity; unicode glyphs fall back to ASCII.
bool gForcePlain = false;

bool plain() { return !colorEnabled(); }

const char* gl(const char* unicode, const char* ascii) {
    return plain() ? ascii : unicode;
}

int panelWidth() { return std::clamp(terminalWidth(), 44, 100); }

// UTF-8 glyphs
const char* kH      = "\u2500";   // ─
const char* kLt     = "\u256d";   // ╭
const char* kRt     = "\u256e";   // ╮
const char* kLb     = "\u2570";   // ╰
const char* kRb     = "\u256f";   // ╯
const char* kVr     = "\u2502";   // │
const char* kBl     = "\u2588";   // █
const char* kEr     = "\u2591";   // ░
const char* kGem    = "\u25c6";   // ◆
const char* kDi     = "\u25b8";   // ▸
const char* kCk     = "\u2713";   // ✓
const char* kSk     = "\u25cb";   // ○
const char* kMed    = "\u25c9";   // ◉
const char* kSword  = "\u2694";   // ⚔

std::string h(int n) {
    const char* ch = gl(kH, "-");
    std::string s;
    s.reserve(3 * static_cast<std::size_t>(std::max(n, 0)));
    for (int i = 0; i < n; ++i) s += ch;
    return s;
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
    }
    return "?";
}

std::string branchName(ClassId c, int b) {
    if (c == ClassId::Warrior) return b == 0 ? "Berserker" : b == 1 ? "Defender" : "Warcry";
    if (c == ClassId::Mage)    return b == 0 ? "Fire"      : b == 1 ? "Frost"    : "Arcane";
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
        titleBar("Sell gear", 33);
        for (std::size_t k = 0; k < gear.size(); ++k) {
            const Item& it = vault.items[static_cast<std::size_t>(gear[k])];
            panelLine(chip(static_cast<int>(k + 1))
                      + color(rarityColor(it.rarity), it.name)
                      + "  " + gold(sellPrice(it)));
        }
        panelLine(dim(chip(0) + "back"));
        panelBottom(33);
        const int pick = io::askInt("Sell which? (0 back)", 0, static_cast<int>(gear.size()));
        if (pick == 0) return false;
        const int idx = gear[static_cast<std::size_t>(pick - 1)];
        const int price = sellPrice(vault.items[static_cast<std::size_t>(idx)]);
        panelLine(color(33, kDi)
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
        titleBar("Salvage gear", 34);
        for (std::size_t k = 0; k < gear.size(); ++k) {
            const Item& it = vault.items[static_cast<std::size_t>(gear[k])];
            std::string val = color(36, std::to_string(salvageShards(it)) + " shards");
            if (salvageEssence(it) > 0) val += color(35, ", 1 essence");
            panelLine(chip(static_cast<int>(k + 1))
                      + color(rarityColor(it.rarity), it.name) + "  " + val);
        }
        panelLine(dim(chip(0) + "back"));
        panelBottom(34);
        const int pick = io::askInt("Salvage which? (0 back)", 0, static_cast<int>(gear.size()));
        if (pick == 0) return false;
        const int idx = gear[static_cast<std::size_t>(pick - 1)];
        vault.shards += salvageShards(vault.items[static_cast<std::size_t>(idx)]);
        vault.essence += salvageEssence(vault.items[static_cast<std::size_t>(idx)]);
        panelLine(color(36, kDi) + " Salvaged "
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
        titleBar("Socket into " + it.name, 35);
        panelLine(color(36, "Free sockets: " + std::to_string(it.freeSockets())));
        for (std::size_t k = 0; k < vault.runes.size(); ++k)
            panelLine(chip(static_cast<int>(k + 1)) + color(35, vault.runes[k].describe()));
        panelLine(dim(chip(0) + "back"));
        panelBottom(35);
        const int pick = io::askInt("Socket which rune? (0 back)", 0,
                                    static_cast<int>(vault.runes.size()));
        if (pick == 0) return false;
        const Rune r = vault.runes[static_cast<std::size_t>(pick - 1)];
        if (socketRune(it, r)) {
            vault.runes.erase(vault.runes.begin() + (pick - 1));
            panelLine(color(35, kDi) + " Socketed " + r.describe() + " into " + it.name);
            std::cout << "\n";
            return true;
        }
        std::cout << dim("  No free sockets.\n");
        return false;
    }
}

bool extractAction(Item& it, Vault& vault) {
    if (it.runes.empty()) {
        std::cout << dim("  Nothing socketed to extract.\n");
        return false;
    }
    const int cost = runeExtractCost(it);
    if (vault.gold < cost) {
        std::cout << dim("  Extraction costs " + std::to_string(cost) + "g. Not enough gold.\n");
        return false;
    }
    vault.gold -= cost;
    const Rune r = extractRune(it);
    vault.runes.push_back(r);
    panelLine(color(35, kDi) + " Extracted " + r.describe() + " (" + gold(cost) + ")");
    std::cout << "\n";
    return true;
}

} // namespace

int terminalWidth() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return static_cast<int>(w.ws_col);
    if (const char* c = std::getenv("COLUMNS")) {
        const int v = std::atoi(c);
        if (v > 0) return v;
    }
    return 80;
}

bool isTty() { return isatty(STDOUT_FILENO) != 0; }

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

std::string stripAnsi(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    const std::size_t n = text.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (text[i] == '\x1b' && i + 1 < n && text[i + 1] == '[') {
            i += 2;
            while (i < n && text[i] != 'm') ++i;
            continue;
        }
        out += text[i];
    }
    return out;
}

std::string center(const std::string& text, int width) {
    if (width <= 0 || static_cast<int>(text.size()) >= width) return text;
    const int pad = width - static_cast<int>(text.size());
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
        case Rarity::Common:    return 37;
        case Rarity::Uncommon:  return 32;
        case Rarity::Rare:      return 36;
        case Rarity::Epic:      return 35;
        case Rarity::Legendary: return 33;
    }
    return 37;
}

std::string meter(int value, int max, int width, int code) {
    if (width <= 0) width = 1;
    double pct = 0.0;
    if (max > 0) pct = static_cast<double>(value) / static_cast<double>(max);
    pct = std::clamp(pct, 0.0, 1.0);
    const int fill = std::clamp(static_cast<int>(std::lround(pct * static_cast<double>(width))),
                                0, width);
    const char* full = gl(kBl, "#");
    const char* empty = gl(kEr, ".");
    std::string s;
    for (int i = 0; i < fill; ++i) s += full;
    for (int i = fill; i < width; ++i) s += empty;
    return color(code, s);
}

std::string hpMeter(int hp, int max, int width) {
    double pct = 0.0;
    if (max > 0) pct = static_cast<double>(hp) / static_cast<double>(max);
    int code = 32;                       // green
    if (pct <= 0.25) code = 31;          // red
    else if (pct <= 0.50) code = 33;     // amber
    return meter(hp, max, width, code);
}

void rule(int code) {
    std::cout << color(code, h(panelWidth() - 2)) << "\n";
}

void titleBar(const std::string& title, int code) {
    std::cout << color(90, h(3)) << " " << bold(color(code, title))
              << " " << color(90, h(3)) << "\n";
}

void panelTop(const std::string& title, int code) {
    const int inner = panelWidth() - 2;
    const int t = std::clamp(static_cast<int>(title.size()), 0, std::max(inner - 4, 4));
    const int L = std::max(0, (inner - t) / 2 - 1);
    const int R = std::max(0, inner - t - L - 2);
    std::cout << color(code,
                       std::string(gl(kLt, "+")) + h(L) + " " + title + " " + h(R)
                       + std::string(gl(kRt, "+")))
              << "\n";
}

void panelLine(const std::string& line) {
    std::cout << color(36, gl(kVr, "|")) << " " << line << "\n";
}

void panelBottom(int code) {
    std::cout << color(code, std::string(gl(kLb, "+")) + h(panelWidth() - 2)
                       + std::string(gl(kRb, "+")))
              << "\n";
}

std::string chip(int n) {
    return color(90, "[") + color(36, std::to_string(n)) + color(90, "] ");
}

std::string gold(int amount) {
    return color(33, std::to_string(amount) + "g");
}

std::string elementTag(core::Element e) {
    switch (e) {
        case core::Element::Fire:   return color(31, "[Fire]");
        case core::Element::Frost:  return color(94, "[Frost]");
        case core::Element::Arcane: return color(35, "[Arcane]");
        default:                    return "";
    }
}

void printHeader() {
    const int w = panelWidth();
    std::cout << color(36, std::string(gl(kLt, "+")) + h(w - 2) + std::string(gl(kRt, "+"))) << "\n";
    std::cout << color(36, std::string(gl(kVr, "|")) + " "
                + center(std::string(kGem) + "  E N D L E S S   R I F T  " + kGem, w - 4) + " "
                + std::string(gl(kVr, "|"))) << "\n";
    std::cout << color(36, std::string(gl(kVr, "|")) + " "
                + center("a CLI dungeon crawler of endless ascent", w - 4) + " "
                + std::string(gl(kVr, "|"))) << "\n";
    std::cout << color(36, std::string(gl(kLb, "+")) + h(w - 2) + std::string(gl(kRb, "+"))) << "\n\n";
}

int mainMenu() {
    while (true) {
        if (!std::cin.good()) return 3;   // EOF → quit instead of spinning
        printHeader();
        panelTop("Main Menu", 36);
        panelLine(color(33, chip(1)) + bold("New Game") + dim("   start a fresh descent"));
        panelLine(chip(2) + bold("Continue") + dim("         load a wanderer"));
        panelLine(chip(3) + bold("Quit") + dim("           leave the Rift"));
        panelBottom();
        const int c = io::askInt("Choose", 1, 3);
        if (c != -1) return c;
    }
}

void saveSlotsList() {
    for (int i = 1; i <= 3; ++i) {
        const auto s = save::peek("save" + std::to_string(i) + ".rpg");
        if (!s.valid) {
            panelLine(chip(i) + dim("Slot " + std::to_string(i) + "  \u2014  empty"));
        } else {
            panelLine(chip(i)
                      + color(rarityColor(Rarity::Rare), className(s.cls))
                      + "  L" + std::to_string(s.level)
                      + color(36, "  \u25b8 Floor " + std::to_string(s.floor)));
        }
    }
}

int chooseSaveSlot(bool newGame) {
    while (true) {
        std::cout << "\n";
        panelTop(newGame ? "New Game" : "Continue", 36);
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
        panelTop("Choose your class", 36);
        panelLine(color(31, chip(1)) + bold("Warrior") + dim("  stamina \u00b7 big hits \u00b7 shields"));
        panelLine(color(34, chip(2)) + bold("Mage")    + dim("  mana \u00b7 elemental spells"));
        panelLine(color(35, chip(3)) + bold("Rogue")   + dim("  energy \u00b7 crits & poison"));
        panelBottom();
        const int c = io::askInt("Class", 1, 3);
        switch (c) {
            case 1: return ClassId::Warrior;
            case 2: return ClassId::Mage;
            case 3: return ClassId::Rogue;
        }
    }
}

void showCharacter(const Character& pc, const Vault& vault) {
    const auto st = pc.stats(vault);
    const std::string title = std::string(className(pc.classId())) + "  \u00b7  Level "
                            + std::to_string(pc.level()) + "  \u00b7  Floor "
                            + std::to_string(pc.currentFloor());
    std::cout << "\n";
    panelTop(title, 36);
    panelLine("HP " + hpMeter(pc.hp(), st.maxHp, 16)
              + "  " + dim(std::to_string(pc.hp()) + "/" + std::to_string(st.maxHp)));
    panelLine(std::string(resourceName(pc.classId())) + " " + meter(pc.resource(), st.maxResource, 16, 34)
              + "  " + dim(std::to_string(pc.resource()) + "/" + std::to_string(st.maxResource)));
    panelLine("XP " + meter(pc.xp(), pc.xpToNext(), 16, 33)
              + "  " + dim(std::to_string(pc.xp()) + "/" + std::to_string(pc.xpToNext()))
              + "   " + color(36, std::to_string(pc.skillPoints()) + " skill pt"));
    panelLine("ATK " + bold(std::to_string(st.attack))
              + "   DEF " + bold(std::to_string(st.defense))
              + "   Crit " + std::to_string(st.critChance) + "%"
              + "(+" + std::to_string(st.critBonus) + ")"
              + "   Leech " + std::to_string(st.lifeStealPct) + "%"
              + "   XP+" + std::to_string(st.xpGainPct) + "%");
    std::string pf = "Passive  " + color(33, bold(passiveName(pc.classId())));
    if (vault.mastery > 0)
        pf += "   " + color(35, std::string(kGem) + " " + std::to_string(vault.mastery) + " Mastery");
    if (vault.aspect > 0)
        pf += "   " + color(32, std::string(kGem) + " " + std::to_string(vault.aspect) + " Aspect");
    panelLine(pf);
    panelLine(color(33, "Gold " + std::to_string(vault.gold))
              + "   " + color(36, "Shards " + std::to_string(vault.shards))
              + "   " + color(35, "Essence " + std::to_string(vault.essence))
              + "   " + color(35, "Runestones " + std::to_string(vault.runes.size())));
    panelLine(dim("Equipped:"));
    for (int s = 0; s < kNumSlots; ++s) {
        const Slot slot = static_cast<Slot>(s);
        const int idx = vault.equippedIndex(slot);
        std::string line = "   " + color(90, std::string(slotName(slot)) + ": ");
        if (vault.hasItem(idx))
            line += color(rarityColor(vault.items[static_cast<std::size_t>(idx)].rarity),
                          vault.items[static_cast<std::size_t>(idx)].name);
        else
            line += dim("(empty)");
        panelLine(line);
    }
    panelBottom();
    std::cout << "\n";
}

void showVault(const Vault& vault) {
    if (vault.items.empty()) {
        panelLine(dim("Vault is empty."));
        return;
    }
    for (std::size_t i = 0; i < vault.items.size(); ++i) {
        const Item& it = vault.items[i];
        bool eq = false;
        for (int s = 0; s < kNumSlots; ++s)
            if (vault.equipped[static_cast<std::size_t>(s)] == static_cast<int>(i)) eq = true;
        panelLine(chip(static_cast<int>(i + 1)) + (eq ? color(32, std::string(kDi) + " ") : dim("  "))
                  + color(rarityColor(it.rarity), it.describe()));
    }
}

void equipMenu(Character& pc, Vault& vault) {
    (void)pc;
    while (true) {
        std::cout << "\n";
        titleBar("Manage gear", 36);
        panelTop("", 90);
        showVault(vault);
        panelLine(dim(chip(0) + "back"));
        panelBottom();
        const int pick = io::askInt("Equip item", 0, static_cast<int>(vault.items.size()));
        if (pick == 0) return;
        const int idx = pick - 1;
        const Item& it = vault.items[static_cast<std::size_t>(idx)];
        if (it.isPot()) {
            std::cout << dim("  That's a potion, not gear.\n");
            continue;
        }
        vault.equip(idx);
        panelLine(color(32, kDi) + " Equipped " + color(rarityColor(it.rarity), it.name));
        std::cout << "\n";
    }
}

void spellBook(const Character& pc) {
    const auto& spells = classSpells(pc.classId());
    const auto tree = pc.tree();
    std::cout << "\n";
    titleBar(std::string("Spell tree \u2014 ") + std::to_string(pc.skillPoints()) + " points", 34);
    for (int b = 0; b < 3; ++b) {
        std::string line = color(33, bold(branchName(pc.classId(), b))) + ":";
        for (int i = b; i < 12; i += 4) {
            const Spell& s = spells[static_cast<std::size_t>(i)];
            if (tree[static_cast<std::size_t>(i)])
                line += "  " + color(33, std::string(kMed) + " " + s.name);
            else
                line += "  " + dim(std::string(kSk) + " " + s.name);
        }
        panelLine(line);
    }
    panelBottom(34);
    std::cout << "\n";
}

void recordsScreen(const Character& pc, const Vault& vault) {
    (void)pc;
    const auto& v = vault;
    std::cout << "\n";
    panelTop("Records & Achievements", 35);
    panelLine(color(36, "Best floor " + std::to_string(v.bestFloor))
              + "   " + color(31, "Bosses " + std::to_string(v.bossesSlain))
              + "   " + color(37, "Kills " + std::to_string(v.kills))
              + "   " + color(90, "Deaths " + std::to_string(v.deaths)));
    panelLine(color(33, "Legendaries " + std::to_string(v.legendaryFound))
              + "   " + color(35, "Mastery " + std::to_string(v.mastery))
              + "   " + color(32, "Ascensions " + std::to_string(v.aspect)));
    panelLine(dim(""));

    struct Ach { const char* name; bool earned; };
    const std::vector<Ach> ach = {
        { "First Blood",            v.kills >= 1 },
        { "Boss Slayer",            v.bossesSlain >= 1 },
        { "Deep Delver",            v.bestFloor >= 25 },
        { "Dungeon Master",         v.bestFloor >= 50 },
        { "Riftbreaker",            v.bestFloor >= 100 },
        { "Legendary Hunter",       v.legendaryFound >= 1 },
        { "Death Is a Teacher",     v.deaths >= 1 },
        { "Master of the Rift",     v.mastery >= 3 },
        { "Aspect of Eternity",     v.aspect >= 1 },
    };
    for (const auto& a : ach)
        panelLine(a.earned ? color(33, std::string(kCk) + " " + a.name)
                           : dim(std::string(kSk) + " " + a.name));
    panelBottom(35);
    std::cout << dim("  (press enter to return)\n");
    std::string line;
    std::getline(std::cin, line);
}

// ---------------------------------------------------------------------------
// camp
// ---------------------------------------------------------------------------

bool spellTrainer(Character& pc) {
    while (true) {
        spellBook(pc);
        std::cout << "  " << dim("Train as ") << color(36, "'<branch> <depth>'") << dim(", e.g. ") 
                  << color(36, "<1 1>") << dim(", or ") << color(36, "<0>") << dim(" to exit\n");
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) continue;
        if (line[0] == '0') return false;
        int b = 0, d = 0;
        if (std::sscanf(line.c_str(), "%d %d", &b, &d) != 2) continue;
        if (b < 1 || b > 3 || d < 1 || d > 4) continue;
        if (pc.spendPoint(b - 1, d - 1))
            panelLine(color(34, kDi) + " Learned " + pc.spell(b - 1, d - 1).name);
        else
            std::cout << dim("  Can't learn that (prerequisites or points).\n");
    }
}

bool blacksmith(Character& pc, Vault& vault, core::Rng& rng) {
    (void)pc;
    while (true) {
        std::cout << "\n";
        titleBar("Blacksmith", 90);
        std::cout << color(33, "  Gold ") << std::to_string(vault.gold)
                  << color(36, "   Shards ") << std::to_string(vault.shards)
                  << color(35, "   Essence ") << std::to_string(vault.essence) << "\n";
        panelTop("", 90);
        if (vault.items.empty()) {
            panelLine(dim("No items."));
            panelBottom();
            return false;
        }
        for (std::size_t i = 0; i < vault.items.size(); ++i) {
            const Item& it = vault.items[i];
            if (it.isPot()) {
                panelLine(chip(static_cast<int>(i + 1))
                          + color(rarityColor(it.rarity), it.describe()));
                continue;
            }
            std::string hints;
            hints += color(37, "  [1] upgrade " + gold(upgradeCost(it)));
            hints += color(94, "  [2] reforge " + gold(reforgeCost(it)));
            if (canAwaken(it)) hints += color(35, "  [3] awaken " + std::to_string(awakenCost(it)) + " es");
            if (it.freeSockets() > 0) hints += color(36, "  [4] socket");
            if (!it.runes.empty())   hints += color(91, "  [5] extract " + gold(runeExtractCost(it)));
            panelLine(chip(static_cast<int>(i + 1))
                      + color(rarityColor(it.rarity), it.describe()));
            panelLine(color(90, "        ") + hints);
        }
        panelLine(dim(chip(0) + "back"));
        panelBottom();
        const int pick = io::askInt("Item", 0, static_cast<int>(vault.items.size()));
        if (pick == 0) return false;
        const int idx = pick - 1;
        Item& it = vault.items[static_cast<std::size_t>(idx)];
        if (it.isPot()) {
            std::cout << dim("  Potions can't be reforged.\n");
            continue;
        }
        const int act = io::askInt("Action (1-5)", 1, 5);
        if (act == 1) {
            if (vault.shards < upgradeCost(it))
                std::cout << dim("  Not enough shards.\n");
            else if (!canUpgrade(it, pc.currentFloor()))
                std::cout << dim("  Item already at the floor cap.\n");
            else {
                vault.shards -= upgradeCost(it);
                upgrade(it);
                panelLine(color(37, kSword) + " Forged to iLvl " + std::to_string(it.iLvl)
                          + color(rarityColor(it.rarity), "  " + it.describe()));
                std::cout << "\n";
            }
        } else if (act == 2) {
            if (vault.gold < reforgeCost(it))
                std::cout << dim("  Not enough gold.\n");
            else {
                vault.gold -= reforgeCost(it);
                reforge(it, rng);
                panelLine(color(94, kDi) + " Rerolled " + color(rarityColor(it.rarity), it.describe()));
                std::cout << "\n";
            }
        } else if (act == 3) {
            if (!canAwaken(it))
                std::cout << dim("  Can't awaken that one.\n");
            else if (vault.essence < awakenCost(it))
                std::cout << dim("  Not enough essence.\n");
            else {
                vault.essence -= awakenCost(it);
                awaken(it, rng);
                panelLine(color(35, std::string(kGem) + " Awakened ") + color(rarityColor(it.rarity), it.describe()));
                std::cout << "\n";
            }
        } else if (act == 4) {
            socketAction(it, vault);
        } else if (act == 5) {
            extractAction(it, vault);
        }
    }
}

bool merchantMenu(Character& pc, Vault& vault, core::Rng& rng) {
    const int floor = pc.currentFloor();
    const int hpPrice = 8 + 2 * floor;
    const int mpPrice = 6 + 2 * floor;
    const int shardPrice = 12;
    const int essencePrice = 30;
    while (true) {
        std::cout << "\n";
        titleBar("Merchant", 33);
        panelTop("", 90);
        panelLine(chip(1) + color(31, "Healing Draught") + "  " + gold(hpPrice));
        panelLine(chip(2) + color(94, "Mana Draught")    + "  " + gold(mpPrice));
        panelLine(chip(3) + color(36, "Shards \u00d73")             + "  " + gold(shardPrice));
        panelLine(chip(4) + color(35, "Essence")           + "  " + gold(essencePrice));
        panelLine(dim(chip(5) + "Sell gear"));
        panelLine(dim(chip(6) + "Salvage gear"));
        panelLine(dim(chip(0) + "back"));
        panelBottom(33);
        const int c = io::askInt("Buy", 0, 6);
        if (c == 0) return false;
        if (c == 5) { sellMenu(vault); continue; }
        if (c == 6) { salvageMenu(vault); continue; }
        Item buy;
        int price = 0;
        if (c == 1) { price = hpPrice; buy = makePotion(floor + rng.roll(0, 2), rng); buy.manaRestore = 0; }
        else if (c == 2) { price = mpPrice; buy = makePotion(floor + rng.roll(0, 2), rng); buy.heal = 0; }
        else if (c == 3) { price = shardPrice; vault.shards += 3; }
        else if (c == 4) { price = essencePrice; vault.essence += 1; }
        if (price > 0) {
            if (vault.gold < price) {
                std::cout << dim("  Not enough gold.\n");
                continue;
            }
            vault.gold -= price;
            if (c == 3 || c == 4) {
                panelLine(color(32, kDi) + " Purchased");
            } else {
                vault.add(buy);
                panelLine(color(33, kDi) + " Bought "
                          + color(rarityColor(buy.rarity), buy.describe()));
            }
            std::cout << "\n";
        }
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
    panelLine(color(36, kDi) + " Tree cleared. " + std::to_string(pc.skillPoints()) + " points refunded");
    std::cout << "\n";
    return true;
}

CampResult camp(Character& pc, Vault& vault, core::Rng& rng) {
    while (true) {
        showCharacter(pc, vault);
        titleBar(std::string("Camp \u2014 Floor ") + std::to_string(pc.currentFloor()), 90);
        std::cout << "\n";
        panelLine(chip(1) + bold("Rest ") + dim("        full recovery"));
        panelLine(chip(2) + bold("Blacksmith ") + dim("     forge \u00b7 reroll \u00b7 sockets"));
        panelLine(chip(3) + bold("Merchant ") + dim("       buy, sell & salvage"));
        panelLine(chip(4) + bold("Manage gear ") + dim("      equip your vault"));
        panelLine(chip(5) + bold("Spell trainer ") + dim("    spend skill points"));
        panelLine(chip(6) + bold("Respec ") + dim("       refund the tree"));
        panelLine(color(33, chip(7) + bold("Descend ") + dim("       head deeper to Floor ")
                  + std::to_string(pc.currentFloor() + 1)));
        panelLine(chip(8) + bold("Records ") + dim("      & achievements"));
        if (pc.currentFloor() >= 100)
            panelLine(color(35, chip(9) + bold("Ascend the Rift ") + dim(" prestige +1")));
        panelLine(dim(chip(0) + "Save & quit"));
        panelBottom();
        const int hi = pc.currentFloor() >= 100 ? 9 : 8;
        const int c = io::askInt("Camp", 0, hi);
        switch (c) {
            case 1: {
                const auto st = pc.stats(vault);
                pc.restoreAll(st.maxHp, st.maxResource);
                panelLine(color(32, kDi) + " You rest and recover completely.");
                std::cout << "\n";
                break;
            }
            case 2: blacksmith(pc, vault, rng); break;
            case 3: merchantMenu(pc, vault, rng); break;
            case 4: equipMenu(pc, vault); break;
            case 5: spellTrainer(pc); break;
            case 6: respecMenu(pc, vault); break;
            case 7: {
                pc.setFloor(pc.currentFloor() + 1);
                const auto st = pc.stats(vault);
                pc.restoreAll(st.maxHp, st.maxResource);
                panelLine(color(33, "\u2193") + " You descend to Floor "
                          + std::to_string(pc.currentFloor()) + "...");
                std::cout << "\n";
                return CampResult::Descend;
            }
            case 8: recordsScreen(pc, vault); break;
            case 9:
                if (pc.currentFloor() >= 100) return CampResult::Ascend;
                break;
            default: return CampResult::Quit;
        }
    }
}

} // namespace rpg::ui