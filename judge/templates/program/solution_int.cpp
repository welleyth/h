#include <cstdio>

int main() {
    int n = 0;
    if (std::scanf("%d", &n) != 1) return 1;
    unsigned sum = 0;
    for (int at = 0; at < n; at++) {
        unsigned value = 0;
        if (std::scanf("%u", &value) != 1) return 1;
        sum += value;
    }
    std::printf("%d\n", static_cast<int>(sum));
}
