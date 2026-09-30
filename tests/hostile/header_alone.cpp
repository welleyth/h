#include "../../eolymp.h"

int main() {
    std::unordered_map<int, int> counted;
    counted[1] = 2;
    std::function<int(int)> const look = [&](int key) { return counted[key]; };
    std::hash<std::string> const hashed;
    auto const bound = std::bind(std::greater<int>(), std::placeholders::_1, 1);
    auto const negated = std::not_fn(bound);
    std::size_t const spread = hashed("eolymp") % 2;
    return std::invoke(look, 1) == 2 && bound(2) && negated(0) && spread < 2 ? 0 : 1;
}
