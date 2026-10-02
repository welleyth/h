#pragma once

#include <cstdint>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"

namespace eo {
namespace shapes {
namespace detail {

#if defined(__SIZEOF_INT128__)
inline std::uint64_t mul_mod(std::uint64_t left, std::uint64_t right, std::uint64_t modulus) {
    __extension__ typedef unsigned __int128 wide;
    return static_cast<std::uint64_t>(static_cast<wide>(left) * right % modulus);
}
#else
inline std::uint64_t mul_mod(std::uint64_t left, std::uint64_t right, std::uint64_t modulus) {
    std::uint64_t product = 0;
    left %= modulus;
    for (; right != 0; right >>= 1) {
        if ((right & 1) != 0) product = product >= modulus - left ? product - (modulus - left) : product + left;
        left = left >= modulus - left ? left - (modulus - left) : left + left;
    }
    return product;
}
#endif

inline std::uint64_t pow_mod(std::uint64_t base, std::uint64_t exponent, std::uint64_t modulus) {
    std::uint64_t result = 1 % modulus;
    base %= modulus;
    for (; exponent != 0; exponent >>= 1) {
        if ((exponent & 1) != 0) result = mul_mod(result, base, modulus);
        base = mul_mod(base, base, modulus);
    }
    return result;
}

inline bool strong_probable_prime(std::uint64_t n, std::uint64_t base) {
    std::uint64_t odd = n - 1;
    int twos = 0;
    while (odd % 2 == 0) {
        odd /= 2;
        twos++;
    }
    std::uint64_t power = pow_mod(base, odd, n);
    if (power == 0 || power == 1 || power == n - 1) return true;
    for (int at = 1; at < twos; at++) {
        power = mul_mod(power, power, n);
        if (power == n - 1) return true;
    }
    return false;
}

inline std::vector<std::uint64_t> const& first_primes() {
    static std::vector<std::uint64_t> const primes{2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    return primes;
}

}  // namespace detail

[[nodiscard]] inline bool is_prime(long long n) {
    if (n < 2) return false;
    std::uint64_t const value = static_cast<std::uint64_t>(n);
    for (std::uint64_t const divisor : detail::first_primes())
        if (value % divisor == 0) return value == divisor;
    for (std::uint64_t const base : detail::first_primes())
        if (!detail::strong_probable_prime(value, base)) return false;
    return true;
}

[[nodiscard]] inline long long next_prime(long long n) {
    long long const largest = 9223372036854775783LL;
    if (n > largest)
        eo::detail::library_error(
            fmt("no prime from {} up fits in a long long; {} is the largest", n, largest));
    long long candidate = n < 2 ? 2 : n;
    while (!is_prime(candidate)) candidate++;
    return candidate;
}

[[nodiscard]] inline long long prev_prime(long long n) {
    if (n < 2) eo::detail::library_error(fmt("no prime is at most {}; 2 is the smallest", n));
    long long candidate = n;
    while (!is_prime(candidate)) candidate--;
    return candidate;
}

[[nodiscard]] inline long long random_prime(rng& draw, long long low, long long high) {
    if (low > high) eo::detail::library_error(fmt("random_prime draws from {}..{}, which is empty", low, high));
    long long const from = low < 2 ? 2 : low;
    std::vector<long long> found;
    if (high >= from && static_cast<unsigned long long>(high) - static_cast<unsigned long long>(from) >= 4096) {
        for (;;) {
            long long const candidate = draw.uniform(from, high);
            if (is_prime(candidate)) return candidate;
        }
    }
    for (long long candidate = from; candidate <= high && candidate >= from; candidate++)
        if (is_prime(candidate)) found.push_back(candidate);
    if (found.empty()) eo::detail::library_error(fmt("no prime lies in {}..{}", low, high));
    return draw.pick(found);
}

}  // namespace shapes
}  // namespace eo
