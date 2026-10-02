EO_TEST(small_arrays_and_strings_are_numbered_in_lexicographic_order) {
    for (long long length = 0; length <= 4; length++)
        for (long long const high : {0LL, 1LL, 2LL}) {
            long long const count = eo::shapes::count_arrays(length, -1, high);
            long long expected = 1;
            for (long long at = 0; at < length; at++) expected *= high + 2;
            EO_CHECK_EQ(count, expected);
            std::vector<std::string> seen;
            for (long long index = 0; index < count; index++) {
                std::vector<long long> const values = eo::shapes::array_at(length, -1, high, index);
                EO_CHECK_EQ(static_cast<long long>(values.size()), length);
                std::string key;
                for (long long const one : values) key += static_cast<char>('a' + one + 1);
                seen.push_back(key);
            }
            EO_CHECK(std::is_sorted(seen.begin(), seen.end()));
            EO_CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
        }
    EO_CHECK_EQ(eo::shapes::count_strings(4, eo::charset("abc")), 81LL);
    std::vector<std::string> words;
    for (long long index = 0; index < 81; index++) words.push_back(eo::shapes::string_at(4, eo::charset("abc"), index));
    EO_CHECK(std::is_sorted(words.begin(), words.end()));
    EO_CHECK(std::adjacent_find(words.begin(), words.end()) == words.end());
    EO_CHECK_EQ(words.front(), std::string("aaaa"));
    EO_CHECK_EQ(words.back(), std::string("cccc"));
    EO_CHECK_EQ(eo::shapes::count_strings(0, eo::charset("ab")), 1LL);
    EO_CHECK_EQ(eo::shapes::count_strings(2, eo::charset("")), 0LL);
    EO_CHECK_EQ(eo::shapes::count_arrays(62, 0, 1), 4611686018427387904LL);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::count_arrays(63, 0, 1); }),
                "there are more arrays of 63 values in 0..1 than a long long counts");
    EO_CHECK_IN(
        refusal([] { (void)eo::shapes::count_arrays(1, -9223372036854775807LL - 1, 9223372036854775807LL); }),
                "there are more arrays of 1 values in -9223372036854775808..9223372036854775807 than a long long counts");
    EO_CHECK_EQ(eo::shapes::count_arrays(0, -9223372036854775807LL - 1, 9223372036854775807LL), 1LL);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::count_arrays(-1, 0, 1); }), "arrays have at least no values, not -1");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::array_at(3, 0, 1, 8); }),
                "the arrays of 3 values in 0..1 are numbered 0..7, not 8");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::array_at(3, 2, 1, 0); }), "arrays draw from 2..1, which is empty");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::string_at(2, eo::charset("ab"), -1); }),
                "the strings of 2 letters over \"ab\" are numbered 0..3, not -1");
}
