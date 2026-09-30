#include <cstdio>

int main() {
    int n = 0, value = 0;
    if (std::scanf("%d %d", &n, &value) != 2) return 1;
    if (n >= 5) return 3;
    std::printf("%d\n", value);
}
