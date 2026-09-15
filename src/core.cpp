#include "core.hpp"

namespace rpg::core {

int Rng::roll(int lo, int hi) {
    if (hi <= lo) return lo;
    std::uniform_int_distribution<int> d(lo, hi);
    return d(gen_);
}

double Rng::roll01() {
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(gen_);
}

std::size_t Rng::pick(std::size_t n) {
    if (n == 0) return 0;
    std::uniform_int_distribution<std::size_t> d(0, n - 1);
    return d(gen_);
}

bool Rng::chance(double p) {
    return roll01() < p;
}

const char* elementName(Element e) {
    switch (e) {
        case Element::None:   return "";
        case Element::Fire:   return "Fire";
        case Element::Frost:  return "Frost";
        case Element::Arcane: return "Arcane";
    }
    return "";
}

const char* statusName(StatusEffect s) {
    switch (s) {
        case StatusEffect::Burn:         return "Burn";
        case StatusEffect::Bleed:        return "Bleed";
        case StatusEffect::Poison:       return "Poison";
        case StatusEffect::Slow:         return "Slow";
        case StatusEffect::Stun:         return "Stun";
        case StatusEffect::ArmorShred:   return "Armor Shred";
        case StatusEffect::Regeneration: return "Regeneration";
        case StatusEffect::Guard:        return "Guard";
        case StatusEffect::Rage:         return "Rage";
        case StatusEffect::Enfeeble:     return "Enfeeble";
        case StatusEffect::Vulnerable:   return "Vulnerable";
        default:                         return "None";
    }
}

} // namespace rpg::core