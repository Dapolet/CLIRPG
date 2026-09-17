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
        while (!line.empty() &&
               (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        char* end = nullptr;
        const long v = std::strtol(line.c_str(), &end, 10);
        if (end == line.c_str() || end == nullptr || *end != '\0') continue;
        if (v >= lo && v <= hi) return static_cast<int>(v);
    }
}

} // namespace rpg::io