#include <cstdio>

int main() {
    int n = 0;
    if (std::scanf("%d", &n) != 1) return 1;
    int low = 1, high = n;
    while (low < high) {
        int const middle = low + (high - low) / 2;
        std::printf("? %d\n", middle);
        std::fflush(stdout);
        char reply[8];
        if (std::scanf("%7s", reply) != 1) return 1;
        if (reply[0] == '=') {
            low = high = middle;
            break;
        }
        if (reply[0] == '<') low = middle + 1;
        else high = middle - 1;
    }
    std::printf("! %d\n", low);
    std::fflush(stdout);
}
