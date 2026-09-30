#include "../../eolymp.h"

#include <cstdio>

int main() {
    double const values[] = {0.1,  1.0 / 3, 2.0 / 3, 1e21, 1e-7, 123456789.125, 5e-324, 1.7976931348623157e308,
                             -0.0, 0.5,     2.5,     1e15 + 0.3};
    for (double const value : values)
        std::printf("%s | %s | %s | %s\n", eo::fmt("{}", value).c_str(), eo::fmt("{}", eo::fixed(value, 3)).c_str(),
                    eo::fmt("{}", eo::fixed(value, 45)).c_str(), eo::fmt("{}", eo::fixed(value, 0)).c_str());
    char const* const texts[] = {"0.1", "3.14159265358979323846", "1e5", "-0.000001",
                                 "123456789012345678901234567890.5"};
    for (char const* const text : texts) {
        eo::detail::real_read const read = eo::detail::parse_real(text, true);
        std::printf("%s -> %s %d %d\n", text, eo::fmt("{}", read.value).c_str(), read.decimals,
                    static_cast<int>(read.problem));
    }
}
