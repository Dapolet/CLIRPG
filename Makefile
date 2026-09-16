# Endless Rift — MSYS2/Windows-compatible Makefile (src/ + build/ layout)
CXX       ?= g++
STD       := -std=c++20
WARN      := -Wall -Wextra -Wpedantic
OPT       := -O2
COMMON    := $(STD) $(WARN) $(OPT)
SRC       := src
BUILD     := build

# Windows binary marker used by CI (msys2 sets OS=Windows_NT)
ifeq ($(OS),Windows_NT)
    EXE_EXT := .exe
else
    EXE_EXT :=
endif

# Sources live in src/; objects + binaries land in build/.
MAIN      := main.cpp
GAME      := game.cpp ui.cpp combat.cpp character.cpp items.cpp save.cpp glory.cpp io.cpp core.cpp
TEST      := tests.cpp
SRCS      := $(MAIN) $(GAME) $(TEST)

GAMEOBJS  := $(addprefix $(BUILD)/,$(GAME:.cpp=.o))
MAINOBJ   := $(addprefix $(BUILD)/,$(MAIN:.cpp=.o))
TESTOBJ   := $(addprefix $(BUILD)/,$(TEST:.cpp=.o))
CFILES    := $(addprefix $(SRC)/,$(SRCS))
GAMECFILES:= $(addprefix $(SRC)/,$(GAME))
RPGCFILES := $(GAMECFILES) $(SRC)/$(MAIN)
ASANTESTF := $(GAMECFILES) $(SRC)/$(TEST)

BIN_NAME  ?= clirpg
RPG       := $(BUILD)/$(BIN_NAME)$(EXE_EXT)
TESTS     := $(BUILD)/tests$(EXE_EXT)
RPG_ASAN  := $(BUILD)/clirpg_asan$(EXE_EXT)
CAS_TST   := $(BUILD)/tests_asan$(EXE_EXT)

all: $(RPG)

$(RPG): $(GAMEOBJS) $(MAINOBJ)
	$(CXX) $(COMMON) $^ -o $@

$(BUILD)/%.o: $(SRC)/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(COMMON) -MMD -MP -c $< -o $@

-include $(BUILD)/*.d

# tests: game sources + tests.cpp (no main.cpp — tests has its own main).
$(TESTS): $(GAMEOBJS) $(TESTOBJ)
	$(CXX) $(COMMON) $^ -o $@

# ASan/UBSan binaries are compiled from source with instrumentation.
$(RPG_ASAN): $(RPGCFILES)
	@mkdir -p $(BUILD)
	$(CXX) $(COMMON) -O1 -g -fsanitize=address,undefined $^ -o $@

$(CAS_TST): $(ASANTESTF)
	@mkdir -p $(BUILD)
	$(CXX) $(COMMON) -O1 -g -fsanitize=address,undefined $^ -o $@

tests: $(TESTS)
	./$(TESTS)

asan: $(RPG_ASAN)

tests_asan: $(CAS_TST)
	./$(CAS_TST)

run: $(RPG)
	./$(RPG)

clean:
	rm -rf $(BUILD)

.PHONY: all tests asan tests_asan run clean