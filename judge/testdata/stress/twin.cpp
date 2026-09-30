#include <iostream>
#include <numeric>
#include <vector>

int main() {
    std::size_t n = 0;
    std::cin >> n;
    std::vector<int> values(n);
    for (int& value : values) std::cin >> value;
    std::cout << std::accumulate(values.begin(), values.end(), 0) << "\n";
}
