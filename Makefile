# Endless Rift — MSYS2/Windows-compatible Makefile (src/ + build/ layout)
CXX       ?= g++
STD       := -std=c++20
WARN      := -Wall -Wextra -Wpedantic $(if $(WERROR),-Werror,)
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

# Windows release builds: statically link the runtime so the distributed .exe
# needs no MSYS2 DLLs (libgcc_s_seh-1.dll / libstdc++-6.dll /
# libwinpthread-1.dll). macOS/Linux keep dynamic linking.
ifeq ($(OS),Windows_NT)
    STATIC_LDFLAGS := -static
else
    STATIC_LDFLAGS :=
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
HDRS      := $(wildcard $(SRC)/*.hpp)

BIN_NAME  ?= clirpg
RPG       := $(BUILD)/$(BIN_NAME)$(EXE_EXT)
TESTS     := $(BUILD)/tests$(EXE_EXT)
RPG_ASAN  := $(BUILD)/clirpg_asan$(EXE_EXT)
CAS_TST   := $(BUILD)/tests_asan$(EXE_EXT)

all: $(RPG)

$(RPG): $(GAMEOBJS) $(MAINOBJ)
	$(CXX) $(COMMON) $(STATIC_LDFLAGS) $^ -o $@

$(BUILD)/%.o: $(SRC)/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(COMMON) -MMD -MP -c $< -o $@

-include $(BUILD)/*.d

# tests: game sources + tests.cpp (no main.cpp — tests has its own main).
$(TESTS): $(GAMEOBJS) $(TESTOBJ)
	$(CXX) $(COMMON) $^ -o $@

# ASan/UBSan binaries are compiled from source with instrumentation.
$(RPG_ASAN): $(RPGCFILES) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(COMMON) -O1 -g -fsanitize=address,undefined $(RPGCFILES) -o $@

$(CAS_TST): $(ASANTESTF) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(COMMON) -O1 -g -fsanitize=address,undefined $(ASANTESTF) -o $@

tests: $(TESTS)
	./$(TESTS)

asan: $(RPG_ASAN)

tests_asan: $(CAS_TST)
	./$(CAS_TST)

COVOBJ   := $(BUILD)/tests_cov$(EXE_EXT)
COVSRCS  := $(GAMECFILES) $(SRC)/$(TEST)
$(COVOBJ): $(COVSRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(STD) -O0 -g --coverage $(GAMECFILES) $(SRC)/$(TEST) -o $@

coverage: $(COVOBJ)
	@rm -f $(BUILD)/*.gcda
	@cd $(BUILD) && ./tests_cov$(EXE_EXT) >/dev/null 2>&1 || true
	@echo "--- gcov summaries (relative paths shown per source file) ---"
	@cd $(BUILD) && for f in $(GAME) $(TEST) $(MAIN); do \
	    if command -v gcov >/dev/null 2>&1; then \
	        gcov -o . ../src/$${f%.cpp}.cpp | rg "Lines executed|Branches executed" | \
	            sed "s/^/  $$f: /"; \
	    else \
	        echo "  (gcov not found — skipping $$f)"; \
	    fi; \
	done


run: $(RPG)
	./$(RPG)

clean:
	rm -rf $(BUILD)

.PHONY: all tests asan tests_asan run clean