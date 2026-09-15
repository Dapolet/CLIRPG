#include "character.hpp"
#include "combat.hpp"
#include "core.hpp"
#include "game.hpp"
#include "items.hpp"
#include "save.hpp"
#include "ui.hpp"

#include <iostream>
#include <random>
#include <string>

using namespace rpg;

int main(int argc, char** argv) {
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
            pc = Character(c);
            vault.clear();
            vault.gold = 50;
            vault.add(makePotion(1, rng));
            vault.add(makePotion(1, rng));
            const auto st = pc.stats(vault);
            pc.restoreAll(st.maxHp, st.maxResource);
            save::write(savePath, pc, vault);
            std::cout << "\nA fresh wanderer enters the Rift...\n";
        } else {
            if (!save::read(savePath, &pc, &vault)) {
                std::cout << "No valid save in that slot.\n";
                continue;
            }
            ui::printHeader();
            std::cout << ui::color(32, "Welcome back, " + std::string(className(pc.classId())) + ".") << "\n";
        }

        const auto persist = [&]() { save::write(savePath, pc, vault); };
        game::adventure(pc, vault, rng, savePath, persist);
    }

    ui::printHeader();
    std::cout << "The Rift awaits your return.\n";
    return 0;
}