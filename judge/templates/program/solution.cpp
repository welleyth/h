#include <cstdio>

int main() {
    int n = 0;
    if (std::scanf("%d", &n) != 1) return 1;
    long long sum = 0;
    for (int at = 0; at < n; at++) {
        long long value = 0;
        if (std::scanf("%lld", &value) != 1) return 1;
        sum += value;
    }
    std::printf("%lld\n", sum);
}
