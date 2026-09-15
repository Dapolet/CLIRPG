#include "io.hpp"

#include <cstdlib>
#include <iostream>

namespace rpg::io {

std::string readLine(const std::string& prompt) {
    std::string line;
    std::cout << prompt;
    std::getline(std::cin, line);
    return line;
}

int askInt(const std::string& prompt, int lo, int hi) {
    while (true) {
        std::cout << prompt << " [" << lo << "-" << hi << "]: ";
        std::string line;
        if (!std::getline(std::cin, line)) return lo;
        const int v = std::atoi(line.c_str());
        if (v >= lo && v <= hi) return v;
    }
}

} // namespace rpg::io