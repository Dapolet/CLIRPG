CXX       ?= g++
STD       := -std=c++20
WARN      := -Wall -Wextra -Wpedantic
OPT       := -O2
COMMON    := $(STD) $(WARN) $(OPT)

# Определяем ОС: Windows_NT устанавливается на Windows
ifeq ($(OS),Windows_NT)
    EXE_EXT := .exe
    RM      := rm -f
else
    EXE_EXT :=
    RM      := rm -f
endif

SRCS      := main.cpp ui.cpp combat.cpp character.cpp items.cpp save.cpp io.cpp core.cpp
OBJS      := $(SRCS:.cpp=.o)
GAMEOBJS  := core.o items.o character.o combat.o save.o ui.o io.o

all: rpg$(EXE_EXT)

rpg$(EXE_EXT): $(OBJS)
	$(CXX) $(COMMON) $(OBJS) -o $@

%.o: %.cpp
	$(CXX) $(COMMON) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d) tests.d

tests: tests.o $(GAMEOBJS)
	$(CXX) $(COMMON) $^ -o $@$(EXE_EXT)
	./$@$(EXE_EXT)

asan:
	$(CXX) $(STD) $(WARN) -g -O1 -fsanitize=address,undefined $(SRCS) -o rpg_asan$(EXE_EXT)

run: rpg$(EXE_EXT)
	./rpg$(EXE_EXT)

clean:
	$(RM) *.o *.d rpg rpg.exe tests tests.exe rpg_asan rpg_asan.exe

.PHONY: all tests asan run clean