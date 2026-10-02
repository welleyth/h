EO_TEST(a_de_bruijn_sequence_holds_every_word_exactly_once) {
    for (char const* letters : {"a", "ab", "abc", "0-9"})
        for (int order = 1; order <= 4; order++) {
            eo::charset const allowed(letters);
            std::string const sequence = eo::shapes::de_bruijn(allowed, order);
            std::size_t kinds = 0;
            for (int one = 0; one < 256; one++) kinds += allowed.has(static_cast<char>(one)) ? 1 : 0;
            std::size_t words = 1;
            for (int at = 0; at < order; at++) words *= kinds;
            EO_CHECK_EQ(sequence.size(), words + static_cast<std::size_t>(order) - 1);
            std::vector<std::string> seen;
            for (std::size_t at = 0; at + static_cast<std::size_t>(order) <= sequence.size(); at++)
                seen.push_back(sequence.substr(at, static_cast<std::size_t>(order)));
            std::sort(seen.begin(), seen.end());
            EO_CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
            EO_CHECK_EQ(seen.size(), words);
            bool inside = true;
            for (char const one : sequence) inside = inside && allowed.has(one);
            EO_CHECK(inside);
        }
    EO_CHECK_EQ(eo::shapes::de_bruijn(eo::charset("01"), 3), std::string("0001011100"));
    EO_CHECK_EQ(eo::shapes::de_bruijn(eo::charset("ab"), 20).size(), static_cast<std::size_t>(1048595));
    EO_CHECK_IN(refusal([] { (void)eo::shapes::de_bruijn(eo::charset("a-z"), 6); }),
                "a de Bruijn sequence of order 6 over 26 letters is 308915776 long; 100000000 is the most");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::de_bruijn(eo::charset(""), 2); }), "charset(\"\") holds no characters");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::de_bruijn(eo::charset("ab"), 0); }),
                "a de Bruijn sequence has an order of at least 1, not 0");
}

namespace {

bool is_lyndon(std::string const& word) {
    for (std::size_t shift = 1; shift < word.size(); shift++)
        if (word.substr(shift) + word.substr(0, shift) <= word) return false;
    return !word.empty();
}

}  // namespace

EO_TEST(a_lyndon_word_is_smaller_than_every_rotation_of_itself) {
    eo::rng draw(801);
    for (char const* letters : {"ab", "abc", "a-z"})
        for (long long length = 1; length <= 40; length++) {
            std::string const word = eo::shapes::lyndon(draw, length, eo::charset(letters));
            EO_CHECK_EQ(static_cast<long long>(word.size()), length);
            EO_CHECK(is_lyndon(word));
        }
    EO_CHECK_EQ(eo::shapes::lyndon(draw, 1, eo::charset("z")), std::string("z"));
    std::string const long_one = eo::shapes::lyndon(draw, 2000, eo::charset("ab"));
    EO_CHECK(is_lyndon(long_one));
    std::vector<std::string> seen;
    for (int round = 0; round < 300; round++) seen.push_back(eo::shapes::lyndon(draw, 5, eo::charset("ab")));
    std::sort(seen.begin(), seen.end());
    seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
    EO_CHECK_EQ(seen.size(), static_cast<std::size_t>(6));
    std::string const drawn = eo::shapes::lyndon(draw, 30, eo::charset("abc"));
    pin_bytes(drawn, 30, 0x54c9124a13d1fa35ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::lyndon(draw, 2, eo::charset("a")); }),
                "a Lyndon word of 2 letters needs two different letters to choose from");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::lyndon(draw, 0, eo::charset("ab")); }),
                "a Lyndon word is at least 1 long, not 0");
}

EO_TEST(abacaba_doubles_around_a_new_letter_at_every_power_of_two) {
    EO_CHECK_EQ(eo::shapes::abacaba(15), std::string("abacabadabacaba"));
    EO_CHECK_EQ(eo::shapes::abacaba(0), std::string(""));
    EO_CHECK_EQ(eo::shapes::abacaba(9), std::string("abacabada"));
    std::string const word = eo::shapes::abacaba((1 << 20) - 1);
    EO_CHECK_EQ(word, std::string(word.rbegin(), word.rend()));
    EO_CHECK_EQ(word[(1 << 19) - 1], 't');
    std::string const capped = eo::shapes::abacaba((1LL << 25) + 5);
    EO_CHECK_EQ(capped[(1 << 24) - 1], 'y');
    EO_CHECK_EQ(capped[(1 << 25) - 1], 'z');
    EO_CHECK_EQ(capped.find_first_not_of("abcdefghijklmnopqrstuvwxyz"), std::string::npos);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::abacaba(-1); }), "an abacaba word is at least empty, not -1 long");
}

namespace {

std::uint64_t overflowing_hash(std::string const& text, std::uint64_t base, std::uint64_t shift, bool forward) {
    std::uint64_t hash = 0;
    std::uint64_t power = 1;
    for (char const one : text) {
        std::uint64_t const letter = static_cast<unsigned char>(one) - shift;
        if (forward) {
            hash += letter * power;
            power *= base;
        } else {
            hash = hash * base + letter;
        }
    }
    return hash;
}

}  // namespace

EO_TEST(thue_morse_twins_collide_under_every_odd_base_modulo_two_to_the_64) {
    eo::rng draw(802);
    for (long long const length : {1024LL, 2048LL, 3 * 1024LL, 50 * 1024LL}) {
        std::pair<std::string, std::string> const twins = eo::shapes::thue_morse_twins(length);
        EO_CHECK_EQ(static_cast<long long>(twins.first.size()), length);
        EO_CHECK_EQ(twins.second.size(), twins.first.size());
        EO_CHECK(twins.first != twins.second);
        EO_CHECK_EQ(twins.first, eo::shapes::thue_morse(length));
        std::vector<std::uint64_t> bases{3, 31, 131, 1000000007, 0xffffffffffffffffull};
        for (int round = 0; round < 20; round++) bases.push_back(draw.next() | 1);
        bool collide = true;
        for (std::uint64_t const base : bases)
            for (std::uint64_t const shift : {0ull, 96ull})
                for (bool const forward : {false, true})
                    collide = collide && overflowing_hash(twins.first, base, shift, forward) ==
                                             overflowing_hash(twins.second, base, shift, forward);
        EO_CHECK(collide);
        EO_CHECK(overflowing_hash(twins.first, 2, 0, false) != overflowing_hash(twins.second, 2, 0, false));
    }
    std::pair<std::string, std::string> const short_pair{eo::shapes::thue_morse(512),
                                                         eo::shapes::thue_morse_twins(1024).second.substr(0, 512)};
    EO_CHECK(overflowing_hash(short_pair.first, 3, 0, false) != overflowing_hash(short_pair.second, 3, 0, false));
    EO_CHECK_IN(refusal([] { (void)eo::shapes::thue_morse_twins(512); }),
                "Thue-Morse twins collide modulo 2^64 at lengths that are multiples of 1024, not 512");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::thue_morse_twins(0); }), "multiples of 1024, not 0");
}

namespace {

std::uint64_t slow_product(std::uint64_t left, std::uint64_t right, std::uint64_t modulus) {
    std::uint64_t product = 0;
    left %= modulus;
    for (; right != 0; right >>= 1) {
        if ((right & 1) != 0) product = product >= modulus - left ? product - (modulus - left) : product + left;
        left = left >= modulus - left ? left - (modulus - left) : left + left;
    }
    return product;
}

std::uint64_t modular_hash(std::string const& text, std::uint64_t base, std::uint64_t modulus, std::uint64_t shift,
                           bool forward) {
    std::uint64_t hash = 0;
    std::uint64_t power = 1;
    for (char const one : text) {
        std::uint64_t const letter = (static_cast<unsigned char>(one) - shift) % modulus;
        if (forward) {
            hash = (hash + slow_product(letter, power, modulus)) % modulus;
            power = slow_product(power, base, modulus);
        } else {
            hash = (slow_product(hash, base, modulus) + letter) % modulus;
        }
    }
    return hash;
}

}  // namespace

EO_TEST(anti_hash_strings_collide_under_every_pair_they_were_built_for) {
    eo::rng draw(803);
    using pairs = std::vector<std::pair<long long, long long>>;
    struct asked {
        pairs hashes;
        char const* letters;
        std::size_t longest;
    };
    for (asked const& one :
         {asked{{{131, 1000000007}}, "a-z", 1024}, asked{{{31, 1000000007}, {37, 998244353}}, "a-z", 16384},
          asked{{{1000003, 2305843009213693951LL}}, "ab", 16384}, asked{{{10, 1000}, {7, 1000}}, "0-9", 4096},
          asked{{{131, 1000000007}, {137, 998244353}, {10007, 1000000009}}, "a-z", 1 << 20},
          asked{{{2, 3}}, "xy", 64}, asked{{{256, 2}}, "ab", 64}, asked{{{-5, 1000000007}}, "a-z", 1024}}) {
        std::pair<std::string, std::string> const twins =
            eo::shapes::anti_hash(draw, one.hashes, eo::charset(one.letters));
        EO_CHECK_EQ(twins.first.size(), twins.second.size());
        EO_CHECK(twins.first != twins.second);
        EO_CHECK(!twins.first.empty() && twins.first.size() <= one.longest);
        bool inside = true;
        for (std::size_t at = 0; at < twins.first.size(); at++)
            inside = inside && eo::charset(one.letters).has(twins.first[at]) &&
                     eo::charset(one.letters).has(twins.second[at]);
        EO_CHECK(inside);
        bool collide = true;
        for (std::pair<long long, long long> const& hash : one.hashes) {
            std::uint64_t const modulus = static_cast<std::uint64_t>(hash.second);
            std::uint64_t const base =
                static_cast<std::uint64_t>((hash.first % hash.second + hash.second) % hash.second);
            for (std::uint64_t const shift : {0ull, 96ull})
                collide = collide && modular_hash(twins.first, base, modulus, shift, false) ==
                                         modular_hash(twins.second, base, modulus, shift, false);
        }
        EO_CHECK(collide);
    }
    std::pair<std::string, std::string> const twins =
        eo::shapes::anti_hash(draw, {{31, 1000000007}, {129032259, 1000000007}}, eo::charset("a-z"));
    EO_CHECK_EQ(modular_hash(twins.first, 31, 1000000007, 0, true),
                modular_hash(twins.second, 31, 1000000007, 0, true));
    std::pair<std::string, std::string> const again =
        eo::shapes::anti_hash(draw, {{31, 1000000007}, {37, 998244353}}, eo::charset("a-z"));
    pin_bytes(again.first + "|" + again.second, 8193, 0x3e9257b64a7b62e1ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::anti_hash(draw, {}, eo::charset("a-z")); }),
                "anti_hash needs at least one (base, modulus) pair to break");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::anti_hash(draw, {{31, 1}}, eo::charset("a-z")); }),
                "hash 1 has modulus 1; a modulus is at least 2");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::anti_hash(draw, {{31, 1000000007}}, eo::charset("a")); }),
                "anti_hash needs two different letters; charset(\"a\") has fewer");
    EO_CHECK_IN(refusal([&] {
                    (void)eo::shapes::anti_hash(
                        draw, {{31, 1000000007}, {37, 1000000007}, {41, 1000000007}, {43, 1000000007}},
                        eo::charset("a-z"));
                }),
                "anti_hash needs more than 10000000 letters to break these 4 hashes at once; break fewer");
}
