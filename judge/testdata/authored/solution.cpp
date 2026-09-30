#include <cstdio>

int main() {
    int n = 0;
    if (std::scanf("%d", &n) != 1) return 1;
    int sum = 0;
    for (int at = 0; at < n; at++) {
        int value = 0;
        if (std::scanf("%d", &value) != 1) return 1;
        sum += value;
    }
    std::printf("%d\n", sum);
}
