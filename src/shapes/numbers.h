#pragma once

#include <algorithm>
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

inline bool power_at_most(long long base, int exponent, long long most) {
    long long power = 1;
    for (int at = 0; at < exponent; at++) {
        if (power > most / base) return false;
        power *= base;
    }
    return true;
}

inline long long whole_root(long long most, int exponent) {
    if (exponent == 1) return most;
    long long low = 1;
    long long high = 3037000500;
    while (low < high) {
        long long const middle = low + (high - low + 1) / 2;
        if (power_at_most(middle, exponent, most)) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return low;
}

struct divisor_record {
    long long value = 1;
    long long count = 1;
};

inline void most_divisors_below(long long most, std::size_t prime, long long value, long long count, int highest,
                                divisor_record& best) {
    if (count > best.count || (count == best.count && value < best.value)) best = divisor_record{value, count};
    static long long const primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59};
    if (prime >= sizeof(primes) / sizeof(primes[0])) return;
    long long const factor = primes[prime];
    for (int exponent = 1; exponent <= highest && value <= most / factor; exponent++) {
        value *= factor;
        most_divisors_below(most, prime + 1, value, count * (exponent + 1), exponent, best);
    }
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

[[nodiscard]] inline long long semiprime(rng& draw, long long most) {
    if (most < 4) eo::detail::library_error(fmt("no semiprime is at most {}; 4 is the smallest", most));
    long long const root = detail::whole_root(most, 2);
    long long const smaller = random_prime(draw, (std::max)(2LL, root / 2), root);
    long long const reach = most / smaller;
    return smaller * random_prime(draw, (std::max)(smaller, reach / 2), reach);
}

[[nodiscard]] inline long long prime_power(rng& draw, long long most, int exponent) {
    if (exponent < 1)
        eo::detail::library_error(fmt("a prime power has an exponent of at least 1, not {}", exponent));
    if (!detail::power_at_most(2, exponent, 9223372036854775807LL))
        eo::detail::library_error(fmt("no prime power p^{} fits in a long long", exponent));
    if (!detail::power_at_most(2, exponent, most))
        eo::detail::library_error(fmt("no prime power p^{} is at most {}; {} is the smallest", exponent, most,
                                      1LL << exponent));
    long long const root = detail::whole_root(most, exponent);
    long long const base = random_prime(draw, (std::max)(2LL, root / 2), root);
    long long power = 1;
    for (int at = 0; at < exponent; at++) power *= base;
    return power;
}

[[nodiscard]] inline long long most_divisors(long long most) {
    if (most < 1) eo::detail::library_error(fmt("most_divisors looks at 1..most, and most is {}", most));
    detail::divisor_record best;
    detail::most_divisors_below(most, 0, 1, 1, 63, best);
    return best.value;
}

[[nodiscard]] inline long long strong_pseudoprime(int bases) {
    static long long const least[] = {2047LL,           1373653LL,          25326001LL,
                                      3215031751LL,     2152302898747LL,    3474749660383LL,
                                      341550071728321LL, 341550071728321LL, 3825123056546413051LL,
                                      3825123056546413051LL, 3825123056546413051LL};
    if (bases < 1) eo::detail::library_error(fmt("strong_pseudoprime takes 1..11 prime bases, not {}", bases));
    if (bases > 11)
        eo::detail::library_error(fmt(
            "the least strong pseudoprime to the first {} prime bases does not fit in a long long; ask for 1..11",
            bases));
    return least[bases - 1];
}

[[nodiscard]] inline long long carmichael(rng& draw, long long most) {
    if (most < 561) eo::detail::library_error(fmt("no Carmichael number is at most {}; 561 is the smallest", most));
    std::vector<long long> found;
    for (long long const classic : {561LL, 1105LL, 2465LL, 2821LL, 6601LL, 8911LL})
        if (classic <= most) found.push_back(classic);
    for (long long step = 1;; step++) {
        long long const first = 6 * step + 1;
        long long const second = 12 * step + 1;
        long long const third = 18 * step + 1;
        if (first > most / second || first * second > most / third) break;
        if (is_prime(first) && is_prime(second) && is_prime(third)) found.push_back(first * second * third);
    }
    return draw.pick(found);
}

}  // namespace shapes
}  // namespace eo
