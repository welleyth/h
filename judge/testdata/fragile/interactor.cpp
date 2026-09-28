#include <cstdio>

int main(int argc, char** argv) {
    if (argc < 3) return 3;
    std::FILE* input = std::fopen(argv[1], "r");
    int n = 0;
    if (input == nullptr || std::fscanf(input, "%d", &n) != 1) return 3;
    std::printf("%d\n", n);
    std::fflush(stdout);
    int answer = 0;
    if (std::scanf("%d", &answer) != 1) return 3;
    std::FILE* output = std::fopen(argv[2], "w");
    if (output == nullptr) return 3;
    std::fprintf(output, "%d\n", answer);
    std::fclose(output);
    return answer == 2 * n ? 0 : 1;
}
