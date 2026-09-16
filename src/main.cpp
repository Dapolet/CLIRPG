#include "character.hpp"
#include "combat.hpp"
#include "core.hpp"
#include "game.hpp"
#include "glory.hpp"
#include "items.hpp"
#include "save.hpp"
#include "ui.hpp"

#include <filesystem>
#include <iostream>
#include <random>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace rpg;

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode))
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--plain" || a == "--no-color" || a == "-p") ui::setForcePlain(true);
    }

    core::Rng rng{ [] {
        std::random_device rd;
        return (static_cast<std::uint64_t>(rd()) << 32) ^ rd();
    }() };

    Character pc(ClassId::Warrior);
    Vault vault;
    std::string savePath = "save1.rpg";

    while (true) {
        const int choice = ui::mainMenu();
        if (choice == 3) break;

        const bool newGame = choice == 1;
        const int slot = ui::chooseSaveSlot(newGame);
        if (slot == 0) continue;
        savePath = "save" + std::to_string(slot) + ".rpg";

        if (newGame) {
            const ClassId c = ui::chooseClass();
            const bool hardcore = ui::chooseMode();
            pc = Character(c);
            pc.setHardcore(hardcore);
            vault.clear();
            vault.gold = 50;
            vault.add(makePotion(1, rng));
            vault.add(makePotion(1, rng));
            const auto st = pc.stats(vault);
            pc.restoreAll(st.maxHp, st.maxResource);
            if (!save::write(savePath, pc, vault))
                std::cout << ui::color(ui::c::bad,
                    "  [!] Could not write the save. Progress may be lost.") << "\n";
            glory::sync(pc, vault);
            std::cout << "\nA fresh wanderer ";
            if (hardcore) {
                std::cout << ui::color(ui::c::bad, ui::bold("chooses the hard path")) << "\n";
                std::cout << ui::dim("  Death is permanent. Fall, and your legend is etched into the Glory track.\n\n");
            } else {
                std::cout << "enters the Rift...\n";
            }
        } else {
            if (!save::read(savePath, &pc, &vault)) {
                if (std::filesystem::exists(savePath))
                    std::cout << "That save is corrupted; the Rift refuses it.\n";
                else
                    std::cout << "No valid save in that slot.\n";
                continue;
            }
            glory::sync(pc, vault);
            ui::printHeader();
            if (pc.hardcore())
                std::cout << ui::color(ui::c::bad, ui::bold("HARDCORE")) << " ";
            std::cout << ui::color(ui::c::good, "Welcome back, " + std::string(className(pc.classId())) + ".") << "\n";
        }

        const auto persist = [&]() { return save::write(savePath, pc, vault); };
        game::adventure(pc, vault, rng, savePath, persist);
    }

    ui::printHeader();
    std::cout << "The Rift awaits your return.\n";
    return 0;
}