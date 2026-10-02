#include "eolymp.h"

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    v.features({"path", "star", "caterpillar"});
    int const n = v.read_int(2, 1000, "n");
    v.read_eoln();
    std::vector<eo::edge> const edges = v.read_tree(n, "edge");
    std::vector<int> degree(static_cast<std::size_t>(n) + 1, 0);
    for (eo::edge const& one : edges) {
        degree[static_cast<std::size_t>(one.u)]++;
        degree[static_cast<std::size_t>(one.v)]++;
    }
    int const most = *std::max_element(degree.begin(), degree.end());
    if (most <= 2) v.saw("path");
    if (most == n - 1) v.saw("star");
}
