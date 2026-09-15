#include "game.hpp"

#include "save.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace rpg::game {

void adventure(Character& pc, Vault& vault, core::Rng& rng, const std::string& savePath,
               const std::function<void()>& persist) {
    const auto persistOrWrite = [&]() {
        if (persist) persist();
        else save::write(savePath, pc, vault);
    };

    while (true) {
        const int floor = pc.currentFloor();
        const bool bossFloor = floor % 5 == 0;
        int encounters = 2 + floor / 4;
        if (encounters > 6) encounters = 6;

        vault.bestFloor = std::max(vault.bestFloor, floor);
        vault.bestiary.addBiome(combat::biomeFor(floor));
        if (rng.chance(0.40)) ui::floorEvent(pc, vault, floor, rng);

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
            ui::panelTop("Floor " + std::to_string(floor) + "  \u00b7  encounter "
                         + std::to_string(i + 1) + "/" + std::to_string(encounters), 36);
            ui::panelLine(ui::dim("  " + std::string(combat::biomeFor(floor))));
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
                persistOrWrite();
                std::cout << ui::color(31,
                    "\nYou have been slain. The Rift's vault endures your death. "
                    "Your body slides back to the camp on Floor " +
                    std::to_string(pc.currentFloor()) + ".\n");
                std::cout << ui::color(31, "Lost " + std::to_string(lost) + " gold on the way out.") << "\n";
                died = true;
                break;
            }

            const int xpGainPct = pc.stats(vault).xpGainPct;
            const int goldGainPct = pc.stats(vault).goldGainPct;
            const int before = pc.level();
            const int gainedXp = res.xp + res.xp * xpGainPct / 100;
            const int gainedGold = res.loot.gold + res.loot.gold * goldGainPct / 100;
            pc.gainXp(res.xp, xpGainPct);
            std::cout << ui::color(32, "Victory!") << " +" << gainedXp << " XP, +"
                      << gainedGold << " gold";
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
            vault.gold += gainedGold;
            vault.totalGoldEarned += gainedGold;
            vault.shards += res.loot.shards;
            vault.essence += res.loot.essence;
            vault.kills += res.kills;
            if (isBossFight) ++vault.bossesSlain;
            if (pc.level() > before)
                std::cout << ui::color(33, "LEVEL UP! You are now level ") +
                                 std::to_string(pc.level()) << " (+1 skill point)\n";
            persistOrWrite();
        }

        if (retreated) {
            std::cout << "You retreat back to the camp.\n";
        }

        if (!died && !retreated && floor % 25 == 0) {
            ++vault.mastery;
            std::cout << ui::color(35, "\nRIFT MASTERY +1")
                      << " \u2014 your power compounds as the Rift deepens.\n";
            persistOrWrite();
        }

        const auto result = ui::camp(pc, vault, rng);
        if (result == ui::CampResult::Quit) {
            persistOrWrite();
            break;
        }

        if (result == ui::CampResult::Ascend) {
            ++vault.aspect;
            ui::choosePerk(vault);
            pc.setFloor(1);
            const auto st = pc.stats(vault);
            pc.restoreAll(st.maxHp, st.maxResource);
            std::cout << ui::color(35, "You tear a new Rift open from Floor 1")
                      << " \u2014 Aspect " << vault.aspect << " heightens the darkness.\n";
            persistOrWrite();
        }

        persistOrWrite();
    }
}

} // namespace rpg::game