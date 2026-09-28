#include <cstdio>
#include <cstring>
#include <string>

int main() {
    char role[16];
    if (std::scanf("%15s", role) != 1) return 1;
    if (std::strcmp(role, "alice") == 0) {
        long long x = 0;
        if (std::scanf("%lld", &x) != 1) return 1;
        std::string code;
        for (int bit = 39; bit >= 0; bit--) code.push_back(((x >> bit) & 1) != 0 ? '1' : '0');
        std::printf("%s\n", code.c_str());
    } else {
        char code[64];
        if (std::scanf("%63s", code) != 1) return 1;
        long long x = 0;
        for (char const* at = code; *at != '\0'; at++) x = x * 2 + (*at - '0');
        std::printf("%lld\n", x);
    }
    std::fflush(stdout);
}
