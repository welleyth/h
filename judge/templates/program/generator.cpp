#include <eolymp.h>

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 100000);
    int const most = g.option<int>("max", 1, 1000000000, 1000000000);
    std::string const fill = g.option<std::string>("fill", {"random", "max"}, "random");

    std::vector<long long> values(static_cast<std::size_t>(n), most);
    if (fill == "random") values = g.rng("values").ints(n, 1, most);

    g.out.line(n);
    g.out.line(values);
}
