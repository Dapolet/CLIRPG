# Achievements — Rift RPG

20 achievements earnable on the account-wide **Glory track** (`glory.rpg`, in the
project folder). They are evaluated from your current character + vault each time
the track is synced (on save, after fights, at the camp, on ascension), and once
unlocked they stay unlocked forever — even after the save slot is overwritten or
the run ends.

When you earn one, the game prints:

```
Achievement unlocked: <name>  (<n>/20)
```

| # | Name                 | How to obtain                                                                                                       |
|---|----------------------|---------------------------------------------------------------------------------------------------------------------|
| 0 | **First Blood**      | Kill at least **1 enemy**. The first kill of your very first fight earns it.                                        |
| 1 | **Boss Slayer**      | Slay your first **boss**. Bosses appear every 5th floor (`floor % 5 == 0`).                                        |
| 2 | **Deep Delver**      | Reach **Floor 25**.                                                                                                  |
| 3 | **Dungeon Master**   | Reach **Floor 50**.                                                                                                  |
| 4 | **Riftbreaker**      | Reach **Floor 100**.                                                                                                 |
| 5 | **Legendary Hunter** | Find your first **Legendary** item (boss fights guarantee an epic+ drop — the most reliable source). |
| 6 | **Death Is a Teacher** | Die at least **once** (any non-hardcore death; hardcore deaths erase the run before the count can persist).        |
| 7 | **Master of the Rift** | Reach **Rift Mastery 3**. Mastery +1 is granted for each full 25-floor lap survived without retreating or dying.  |
| 8 | **Aspect of Eternity** | **Ascend** once (camp → Ascend, opening a new Rift from Floor 1 with a higher Aspect).                             |
| 9 | **Slayer**           | Amass **100 kills** across your run.                                                                                 |
| 10 | **Midas**            | Earn **10,000 gold** over the run (`totalGoldEarned` — combat, floor events, and apothecary loot all count).         |
| 11 | **Hardcore Heart**   | Opt into a **hardcore** run at new-game character creation and finish the intro save.                               |
| 12 | **From the Ashes**   | Be on a **hardcore** run that reaches **Floor 25**.                                                                  |
| 13 | **Immortal**         | Be on a **hardcore** run that reaches **Floor 50**.                                                                  |
| 14 | **Pay the Iron Price** | **Die on a hardcore run** (permadeath). Granted at the moment of death via the Glory fall track.                   |
| 15 | **Warden of the Rift** | Slay every one of the **10 named bosses** (the codex unlocks one entry per name killed).                          |
| 16 | **Rift Cartographer** | Visit each of the **12 biomes** (one biome entry per name visited).                                                 |
| 17 | **Collector of Nightmares** | Encounter all **9 enemy affixes** (one entry per affix seen).                                                   |
| 18 | **Spellmaster**       | Learn **all 12 spells** in a single run (the full spell tree).                                                      |
| 19 | **Infinite Descent**  | Reach **Floor 150** (deep in the endless descent).                                                                  |

Notes
- Hardcore achievements (11–13) grant the moment the qualifying character exists
  on the chosen floor; dying erases the slot but the achievement is already banked.
- `Pay the Iron Price` (14) is the only achievement not checked by the regular
  sync rule — it is awarded directly by the permadeath (`fall`) path, and also
  appears in the **Fallen heroes** section of the Records screen.