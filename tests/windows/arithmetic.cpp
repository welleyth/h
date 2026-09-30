#include "../../eolymp.h"

#include <climits>
#include <cstdio>

int main() {
    long long const values[] = {LLONG_MIN, LLONG_MIN + 1, -3037000500LL, -3037000499LL, -4294967296LL, -2, -1, 0,
                                1,         2,             3037000499LL,  3037000500LL,  4294967296LL,  LLONG_MAX - 1,
                                LLONG_MAX};
    for (long long const left : values)
        for (long long const right : values) {
            long long sum = 0;
            long long difference = 0;
            long long product = 0;
            bool const summed = eo::detail::sum_overflows(left, right, &sum);
            bool const subtracted = eo::detail::difference_overflows(left, right, &difference);
            bool const multiplied = eo::detail::product_overflows(left, right, &product);
            std::printf("%lld %lld: %d %lld, %d %lld, %d %lld\n", left, right, summed, sum, subtracted, difference,
                        multiplied, product);
        }
}
