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
