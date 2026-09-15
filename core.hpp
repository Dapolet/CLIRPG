#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <random>

namespace rpg::core {

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

class Rng {
public:
    explicit Rng(std::uint64_t seed) : gen_(seed) {}

    std::uint64_t next() { return gen_(); }
    int roll(int lo, int hi);          // inclusive
    double roll01();
    std::size_t pick(std::size_t n);
    bool chance(double p);

private:
    std::mt19937_64 gen_;
};

constexpr double kGrowRate = 1.07;
constexpr double kXpBase   = 40.0;
constexpr double kXpExp    = 1.6;

inline double aspectScale(int aspect) {
    return 1.0 + 0.15 * static_cast<double>(aspect > 0 ? aspect : 0);
}

enum class Element { None, Fire, Frost, Arcane };

const char* elementName(Element e);

// Fire > Frost > Arcane > Fire. Same element resists; without elements neutral.
inline double elementMult(Element atk, Element def) {
    if (atk == Element::None || def == Element::None) return 1.0;
    if (atk == def) return 0.7;
    if ((atk == Element::Fire   && def == Element::Frost) ||
        (atk == Element::Frost  && def == Element::Arcane) ||
        (atk == Element::Arcane && def == Element::Fire))
        return 1.3;
    return 1.0;
}

inline int xpNeeded(int level) {
    return static_cast<int>(std::ceil(kXpBase * std::pow(level, kXpExp)));
}

inline double enemyScale(int floor) {
    if (floor <= 1) return 1.0;
    return std::exp(std::log(kGrowRate) * (floor - 1));
}

inline double mitigate(double raw, int defense) {
    if (raw <= 0.0) return 0.0;
    return raw * 100.0 / (100.0 + static_cast<double>(defense));
}

inline double critValue(double raw, int critBonus) {
    return raw * (1.5 + static_cast<double>(critBonus) / 100.0);
}

enum class StatusEffect {
    None, Burn, Bleed, Poison,
    Slow, Stun, ArmorShred,
    Regeneration, Guard, Rage,
};

const char* statusName(StatusEffect s);

} // namespace rpg::core