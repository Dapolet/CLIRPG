# Progress.md  -  handoff state (resume point for next session)

Project : C++20 CLI turn-based RPG (rpg)
Path    : /Users/Dapolet/VSCode/RPG   (root)

IMPORTANT: this file is a resume aid ONLY. The source tree is the ground
truth. If you doubt anything, read the files (src/*.cpp/.hpp) before acting.
Compiler: msys2 GCC ("Windows CI") is the acceptance gate; Apple clang is local.

LAST COMMIT (this session): spell tree widened 9->12 per class with a new
depth-4 capstone debuff spell per class; save format bumped v5->v6; full
src/ + build/ + docs/ restructure with a finished VPATH Makefile; build
artifacts untracked via .gitignore; CI paths updated. Full gate is GREEN:
11921 checks / 0 failures (tests + tests_asan); asan clean; --plain clean.

============================================================================
== 1. STATE AS OF THIS HANDOFF (post-commit tree)                          ==
============================================================================
- Layout:
    src/   all C++ sources (main, ui, combat, character, items, save, io,
           core, bestiary.hpp, tests.cpp)
    build/ generated objects + binaries (gitignored)
    docs/  README.md + PLAN.md (v6)
    Makefile: targets all / tests / asan / tests_asan / run / clean.
- Spell tree is 12 spells/class, index = branch*4 + depth (4 capstones per
  class). New depth-3 (UI depth 4) capstones:
    Warrior: Bloodrage, Bulwark, Cripple        (Cripple = Enfeeble 35%/3t)
    Mage:    Pyroclasm, Absolute Zero, Hex      (Hex = Vulnerable 30%/3t)
    Rogue:   Shadow Dance, Crippling Venom, Masterstroke
                                              (Crippling Venom = Enfeeble 30%+Poison)
- Enfeeble/Vulnerable fully wired: Spell.enemyAtkDownPct / enemyVulnPct;
  vulnMult applied in dealAttack + dealSpell; Enfeeble applied in the enemy
  phase; statusName has cases. Each class has a debuff spell (test-enforced).
- Save format: RPGSAVE v6. `unlocked` serializes 12 bits. v5 and older
  saves are REJECTED cleanly (no migration) - intentional.
- Duplicated 3x applyStatus block (previous edit corruption) reduced to 1x.

============================================================================
== 2. REGRESSION GUARDS (keep them)                                        ==
============================================================================
- Spell{...} designator order MUST match the struct in src/character.hpp
  (name, type, cost, cooldown, potency, lvlScale, hits, healPower,
  buffAttack, buffDefense, buffTurns, effect, effectTurns, armorShred,
  stunChancePct, critBonusSelf, element, enemyAtkDownPct, enemyVulnPct).
  msys2 GCC hard-errors on out-of-order designators; clang does not, so the
  bug ONLY shows up in Windows CI. A python scanner (grep 'Spell{'; compare
  designator ranks) must report 0 violations before committing.
- tests.cpp asserts: 12 spells/class, Mage branch elements (0-3 Fire,
  4-7 Frost, 8-11 Arcane), warrior heals at index Bastion[6] and Rally[9],
  every class has >=1 debuff spell (enemyAtkDownPct or enemyVulnPct > 0).
  Keep those indices in sync when adding spells.

============================================================================
== 3. VERIFICATION GATE (run in a fresh session, then note the numbers)    ==
============================================================================
  make all             # 0 warnings, 0 errors
  make tests           # 11921 checks, 0 failures (as of this handoff)
  make asan && ./build/rpg_asan   # ASan/UBSan clean, exit 0
  make tests_asan      # 11921 checks, 0 failures
  ./build/rpg --plain  # exit 0, no ANSI escape bytes
  ls *.rpg | wc -l     # expect 0 stray saves
  git status           # only intended changes present

============================================================================
== 4. SAFE-WRITER (REQUIRED TECHNIQUE for edits - do NOT deviate)          ==
============================================================================
  - File edits: python3 byte-exact replace with assert-guarded anchors:
        seg = open(path,'rb').read()
        assert seg.count(old.encode()) == 1     # MUST be exactly 1
        seg = seg.replace(old.encode(), new.encode())
        assert seg.count(new.encode()) == 1
        open(path,'wb').write(seg)
  - `old` must be copied byte-for-byte from a read of THAT file, not from
    memory. Verify with read after every edit.
  - Non-ASCII: src/*.cpp currently contains ONLY the pre-existing UTF-8
    em-dash in character.cpp's "Rift Mastery \xe2\x80\x94 the endless-mode
    lever" comment (and combat.cpp/ui.cpp hold intentional typographic
    glyphs like '\xc2\xb7'). Never introduce new non-ASCII bytes; scan
    after each edit. docs/ (PLAN/README) legitimately uses typography.
  - Multi-line insertions: paste as a python list of lines joined by '\n',
    never a raw heredoc.

============================================================================
== 5. KNOWN NEXT-STEP IDEAS (not started, need user direction)             ==
============================================================================
  - Tree widening was the user-approved path; if more spells are wanted,
    the 12-slot layout is the new constraint (index = branch*4 + depth).
  - Tests count grew 11912 -> 11921; docs reflect 11921.