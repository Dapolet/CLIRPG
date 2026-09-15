#include "character.hpp"
#include "combat.hpp"
#include "core.hpp"
#include "items.hpp"
#include "save.hpp"
#include "ui.hpp"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

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

        // ---- adventure loop ----
        while (true) {
            const int floor = pc.currentFloor();
            const bool bossFloor = floor % 5 == 0;
            int encounters = 2 + floor / 4;
            if (encounters > 6) encounters = 6;

            vault.bestFloor = std::max(vault.bestFloor, floor);

            bool retreated = false;
            bool died = false;
            for (int i = 0; i < encounters; ++i) {
                std::vector<combat::Enemy> foes;
                const bool isBossFight = bossFloor && i == encounters - 1;
                if (isBossFight)
                    foes = { combat::makeBoss(floor, rng, vault.aspect) };
                else
                    foes = combat::makeEncounter(floor, rng, vault.aspect);

                std::cout << "\n";
                ui::panelTop("Floor " + std::to_string(floor) + "  ·  encounter "
                             + std::to_string(i + 1) + "/" + std::to_string(encounters), 36);
                if (isBossFight) ui::panelLine(ui::color(31, ui::bold("  <BOSS FIGHT>")));
                ui::panelBottom();

                const auto res = combat::fight(pc, vault, foes, rng);

                if (res.fled) {
                    retreated = true;
                    break;
                }
                if (!res.won) {
                    const int lost = vault.gold / 5;
                    save::applyDeath(pc, vault);
                    save::write(savePath, pc, vault);
                    std::cout << ui::color(31,
                        "\nYou have been slain. The Rift's vault endures your death. "
                        "Your body slides back to the camp on Floor " +
                        std::to_string(pc.currentFloor()) + ".\n");
                    std::cout << ui::color(31, "Lost " + std::to_string(lost) + " gold on the way out.") << "\n";
                    died = true;
                    break;
                }

                const int xpGainPct = pc.stats(vault).xpGainPct;
                const int before = pc.level();
                const int gainedXp = res.xp + res.xp * xpGainPct / 100;
                pc.gainXp(res.xp, xpGainPct);
                std::cout << ui::color(32, "Victory!") << " +" << gainedXp << " XP, +"
                          << res.loot.gold << " gold";
                if (res.loot.shards) std::cout << ", +" << res.loot.shards << " shards";
                if (res.loot.essence) std::cout << ", +" << res.loot.essence << " essence";
                std::cout << "\n";
                for (const auto& it : res.loot.items) {
                    std::cout << "    loot: " << ui::color(ui::rarityColor(it.rarity),
                                                                it.describe()) << "\n";
                    if (it.rarity == Rarity::Legendary) ++vault.legendaryFound;
                    vault.add(it);
                }
                for (const auto& r : res.loot.runes) {
                    std::cout << "    rune: " << ui::color(33, r.describe()) << "\n";
                    vault.runes.push_back(r);
                }
                vault.gold += res.loot.gold;
                vault.totalGoldEarned += res.loot.gold;
                vault.shards += res.loot.shards;
                vault.essence += res.loot.essence;
                vault.kills += res.kills;
                if (isBossFight) ++vault.bossesSlain;
                if (pc.level() > before)
                    std::cout << ui::color(33, "LEVEL UP! You are now level ") +
                                     std::to_string(pc.level()) << " (+1 skill point)\n";
                save::write(savePath, pc, vault);
            }

            if (retreated) {
                std::cout << "You retreat back to the camp.\n";
            }

            if (!died && !retreated && floor % 25 == 0) {
                ++vault.mastery;
                std::cout << ui::color(35, "\nRIFT MASTERY +1")
                          << " — your power compounds as the Rift deepens.\n";
                save::write(savePath, pc, vault);
            }

            const auto result = ui::camp(pc, vault, rng);
            if (result == ui::CampResult::Quit) break;

            if (result == ui::CampResult::Ascend) {
                ++vault.aspect;
                pc.setFloor(1);
                const auto st = pc.stats(vault);
                pc.restoreAll(st.maxHp, st.maxResource);
                std::cout << ui::color(35, "You tear a new Rift open from Floor 1")
                          << " — Aspect " << vault.aspect << " heightens the darkness.\n";
                save::write(savePath, pc, vault);
            }

            save::write(savePath, pc, vault);
        }
    }

    ui::printHeader();
    std::cout << "The Rift awaits your return.\n";
    return 0;
}