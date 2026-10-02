namespace {

std::vector<bool> sieve_to(int most) {
    std::vector<bool> prime(static_cast<std::size_t>(most) + 1, true);
    prime[0] = false;
    if (most >= 1) prime[1] = false;
    for (int at = 2; static_cast<long long>(at) * at <= most; at++)
        if (prime[static_cast<std::size_t>(at)])
            for (int multiple = at * at; multiple <= most; multiple += at)
                prime[static_cast<std::size_t>(multiple)] = false;
    return prime;
}

long long smallest_factor(long long n) {
    for (long long at = 2; at * at <= n; at++)
        if (n % at == 0) return at;
    return n;
}

}  // namespace

EO_TEST(primality_agrees_with_a_sieve_and_with_the_known_large_primes) {
    std::vector<bool> const prime = sieve_to(200000);
    bool agrees = true;
    for (int n = 0; n <= 200000; n++)
        agrees = agrees && eo::shapes::is_prime(n) == prime[static_cast<std::size_t>(n)];
    EO_CHECK(agrees);
    EO_CHECK(!eo::shapes::is_prime(-7));
    for (long long const known : {1000000007LL, 998244353LL, 2305843009213693951LL, 9223372036854775783LL,
                                  4294967291LL, 1000000000000000003LL, 3037000493LL})
        EO_CHECK(eo::shapes::is_prime(known));
    for (long long const composite : {2047LL, 1373653LL, 25326001LL, 3215031751LL, 2152302898747LL,
                                      3474749660383LL, 341550071728321LL, 3825123056546413051LL, 561LL, 1105LL,
                                      9223371994482243049LL, 999999999999999999LL, 9223372036854775807LL,
                                      1000000016000000063LL})
        EO_CHECK(!eo::shapes::is_prime(composite));
    EO_CHECK_EQ(smallest_factor(3037000493LL), 3037000493LL);
    EO_CHECK_EQ(3037000493LL * 3037000493LL, 9223371994482243049LL);
}

EO_TEST(the_next_and_the_previous_prime_skip_exactly_the_composites) {
    std::vector<bool> const prime = sieve_to(20000);
    bool agrees = true;
    for (int n = 0; n <= 19000; n++) {
        long long expected = std::max(n, 2);
        while (!prime[static_cast<std::size_t>(expected)]) expected++;
        agrees = agrees && eo::shapes::next_prime(n) == expected;
        if (n >= 2) {
            long long before = n;
            while (!prime[static_cast<std::size_t>(before)]) before--;
            agrees = agrees && eo::shapes::prev_prime(n) == before;
        }
    }
    EO_CHECK(agrees);
    EO_CHECK_EQ(eo::shapes::next_prime(1000000000), 1000000007LL);
    EO_CHECK_EQ(eo::shapes::prev_prime(1000000000), 999999937LL);
    EO_CHECK_EQ(eo::shapes::next_prime(1000000000000000000LL), 1000000000000000003LL);
    EO_CHECK_EQ(eo::shapes::prev_prime(1000000000000000000LL), 999999999999999989LL);
    EO_CHECK_EQ(eo::shapes::prev_prime(9223372036854775807LL), 9223372036854775783LL);
    EO_CHECK_EQ(eo::shapes::next_prime(9223372036854775783LL), 9223372036854775783LL);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::next_prime(9223372036854775784LL); }),
                "no prime from 9223372036854775784 up fits in a long long; 9223372036854775783 is the largest");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::prev_prime(1); }), "no prime is at most 1; 2 is the smallest");
}

EO_TEST(a_random_prime_is_prime_inside_its_range_and_reaches_every_one) {
    eo::rng draw(901);
    std::vector<long long> seen;
    for (int round = 0; round < 2000; round++) {
        long long const one = eo::shapes::random_prime(draw, 1, 100);
        EO_CHECK(one >= 2 && one <= 100 && eo::shapes::is_prime(one));
        seen.push_back(one);
    }
    std::sort(seen.begin(), seen.end());
    seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
    EO_CHECK_EQ(seen.size(), static_cast<std::size_t>(25));
    for (int round = 0; round < 200; round++) {
        long long const big = eo::shapes::random_prime(draw, 1000000000000000000LL, 2000000000000000000LL);
        EO_CHECK(big >= 1000000000000000000LL && big <= 2000000000000000000LL && eo::shapes::is_prime(big));
    }
    EO_CHECK_EQ(eo::shapes::random_prime(draw, 24, 29), 29LL);
    long long const top = eo::shapes::random_prime(draw, 9223372036854775000LL, 9223372036854775807LL);
    EO_CHECK(top >= 9223372036854775000LL && eo::shapes::is_prime(top));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::random_prime(draw, -5, 1); }), "no prime lies in -5..1");
    std::string drawn;
    for (int round = 0; round < 10; round++) drawn += eo::fmt("{};", eo::shapes::random_prime(draw, 1, 1000000000));
    pin_bytes(drawn, 100, 0x69f085838cc53281ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::random_prime(draw, 24, 28); }), "no prime lies in 24..28");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::random_prime(draw, 30, 20); }),
                "random_prime draws from 30..20, which is empty");
}

EO_TEST(a_semiprime_has_two_prime_factors_both_near_the_square_root) {
    eo::rng draw(902);
    for (long long const most : {4LL, 10LL, 1000LL, 1000000000LL, 1000000000000LL}) {
        for (int round = 0; round < 20; round++) {
            long long const n = eo::shapes::semiprime(draw, most);
            long long const p = smallest_factor(n);
            long long const q = n / p;
            EO_CHECK(n <= most && eo::shapes::is_prime(p) && eo::shapes::is_prime(q) && p * q == n);
            long long root = 1;
            while ((root + 1) * (root + 1) <= most) root++;
            EO_CHECK(p * 2 >= root);
        }
    }
    for (int round = 0; round < 20; round++) {
        long long const n = eo::shapes::semiprime(draw, 1000000000000000000LL);
        EO_CHECK(n <= 1000000000000000000LL && n > 100000000000000000LL && !eo::shapes::is_prime(n));
        bool rough = true;
        for (long long divisor = 2; divisor < 100000; divisor++) rough = rough && n % divisor != 0;
        EO_CHECK(rough);
    }
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::semiprime(draw, 3); }), "no semiprime is at most 3; 4 is the smallest");
}

EO_TEST(a_prime_power_is_a_large_prime_raised_to_the_power_asked_for) {
    eo::rng draw(903);
    for (int const k : {1, 2, 3, 5, 62}) {
        long long const most = k == 62 ? 9223372036854775807LL : 1000000000000000000LL;
        long long const n = eo::shapes::prime_power(draw, most, k);
        EO_CHECK(n <= most);
        long long root =
            k == 1 ? n : std::max(2LL, static_cast<long long>(std::pow(static_cast<double>(n), 1.0 / k)) - 2);
        auto const fits = [&](long long base) {
            long long power = 1;
            for (int at = 0; at < k; at++) {
                if (power > n / base) return false;
                power *= base;
            }
            return true;
        };
        while (k > 1 && fits(root + 1)) root++;
        long long check = 1;
        for (int at = 0; at < k; at++) check *= root;
        EO_CHECK_EQ(check, n);
        EO_CHECK(eo::shapes::is_prime(root));
        if (k == 1) EO_CHECK(n > most / 2);
    }
    EO_CHECK_EQ(eo::shapes::prime_power(draw, 4, 2), 4LL);
    std::string drawn;
    for (int round = 0; round < 5; round++) drawn += eo::fmt("{};", eo::shapes::semiprime(draw, 1000000000000LL));
    drawn += eo::fmt("{};", eo::shapes::prime_power(draw, 1000000000000LL, 2));
    pin_bytes(drawn, 78, 0xfd3ea7c0d6e1faa8ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::prime_power(draw, 7, 3); }),
                "no prime power p^3 is at most 7; 8 is the smallest");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::prime_power(draw, 9223372036854775807LL, 63); }),
                "no prime power p^63 fits in a long long");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::prime_power(draw, 100, 0); }),
                "a prime power has an exponent of at least 1, not 0");
}
