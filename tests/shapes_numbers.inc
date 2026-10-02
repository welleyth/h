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
