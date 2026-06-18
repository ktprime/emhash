#include <iostream>
#include <string>
#include "emhash/hash_table8.hpp"

// Custom hash for case-insensitive string keys
struct CaseInsensitiveHash {
    size_t operator()(const std::string& s) const noexcept {
        size_t h = 0;
        for (char c : s) {
            h = h * 131 + static_cast<unsigned char>(std::tolower(c));
        }
        return h;
    }
};

struct CaseInsensitiveEqual {
    bool operator()(const std::string& a, const std::string& b) const {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++) {
            if (std::tolower(a[i]) != std::tolower(b[i])) return false;
        }
        return true;
    }
};

int main() {
    emhash8::HashMap<std::string, int, CaseInsensitiveHash, CaseInsensitiveEqual> map;

    map["Hello"] = 1;
    map["World"] = 2;

    // These lookups are case-insensitive
    std::cout << "map[\"hello\"] = " << map["hello"] << "\n";
    std::cout << "map[\"WORLD\"] = " << map["WORLD"] << "\n";
    std::cout << "map[\"HeLLo\"] = " << map["HeLLo"] << "\n";

    // Enum struct as key example
    enum class Color { Red, Green, Blue };

    struct ColorHash {
        size_t operator()(Color c) const noexcept { return static_cast<size_t>(c); }
    };

    emhash8::HashMap<Color, const char*, ColorHash> colors;
    colors[Color::Red] = "red";
    colors[Color::Green] = "green";
    colors[Color::Blue] = "blue";

    std::cout << "Color::Red = " << colors[Color::Red] << "\n";

    return 0;
}
