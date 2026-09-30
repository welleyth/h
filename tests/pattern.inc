namespace {

std::string refusal_of(std::string const& text) {
    eot::outcome const made = eot::run_until_stop([&] { eo::pattern refused(text); });
    return made.stopped ? made.text : std::string("built");
}

bool matched(char const* pattern, std::string const& token) { return eo::pattern(pattern).matches(token); }

}  // namespace

EO_TEST(a_pattern_matches_the_whole_token) {
    eo::pattern const letters("[a-z]{1,10}");
    EO_CHECK(letters.matches("abc"));
    EO_CHECK(letters.matches("abcdefghij"));
    EO_CHECK(!letters.matches(""));
    EO_CHECK(!letters.matches("abcdefghijk"));
    EO_CHECK(!letters.matches("abC"));
    EO_CHECK(!letters.matches(" abc"));
    EO_CHECK_EQ(letters.text(), std::string("[a-z]{1,10}"));
    EO_CHECK(matched("abc", "abc"));
    EO_CHECK(!matched("abc", "abcd"));
    EO_CHECK(!matched("abc", "ab"));
    EO_CHECK(matched("", ""));
    EO_CHECK(!matched("", "a"));
}

EO_TEST(a_pattern_has_classes_ranges_negation_and_escapes) {
    EO_CHECK(matched("[^a-y]", "z"));
    EO_CHECK(!matched("[^a-y]", "b"));
    EO_CHECK(matched("[^a]", "\xff"));
    EO_CHECK(matched("[a\\-z]", "-"));
    EO_CHECK(!matched("[a\\-z]", "b"));
    EO_CHECK(matched("[-a]", "-"));
    EO_CHECK(matched("[a-]", "-"));
    EO_CHECK(matched("[a-cx-z0-9_]{6}", "bx_9z0"));
    EO_CHECK(matched("[\\]\\\\]{2}", "]\\"));
    EO_CHECK(matched("[[.]{2}", "[."));
    EO_CHECK(matched("\\.", "."));
    EO_CHECK(!matched("\\.", "x"));
    EO_CHECK(matched("a.b", "a.b"));
    EO_CHECK(!matched("a.b", "axb"));
    EO_CHECK(matched("a\\ b", "a b"));
    EO_CHECK(matched("[a-z ]{1,5}", "ab c"));
    EO_CHECK(matched("\\(\\)\\[\\]\\{\\}\\|\\?\\*\\+\\^\\$\\\\", "()[]{}|?*+^$\\"));
    EO_CHECK(matched("[\\^a]", "^"));
    EO_CHECK(matched("[a^]", "^"));
}

EO_TEST(a_pattern_repeats_with_counts_and_symbols) {
    EO_CHECK(matched("a?", ""));
    EO_CHECK(matched("a?", "a"));
    EO_CHECK(!matched("a?", "aa"));
    EO_CHECK(matched("a*", ""));
    EO_CHECK(matched("a*", "aaaa"));
    EO_CHECK(!matched("a+", ""));
    EO_CHECK(matched("a+", "aaa"));
    EO_CHECK(matched("a{3}", "aaa"));
    EO_CHECK(!matched("a{3}", "aa"));
    EO_CHECK(!matched("a{3}", "aaaa"));
    EO_CHECK(matched("a{2,}", "aaaaa"));
    EO_CHECK(!matched("a{2,}", "a"));
    EO_CHECK(matched("a{0}", ""));
    EO_CHECK(matched("a{0,0}b", "b"));
    EO_CHECK(matched("a{007}", "aaaaaaa"));
    EO_CHECK(matched("[1-9][0-9]{0,8}|0", "0"));
    EO_CHECK(matched("[1-9][0-9]{0,8}|0", "123456789"));
    EO_CHECK(!matched("[1-9][0-9]{0,8}|0", "1234567890"));
    EO_CHECK(!matched("[1-9][0-9]{0,8}|0", "01"));
}

EO_TEST(a_pattern_has_alternatives_and_groups) {
    EO_CHECK(matched("YES|NO", "YES"));
    EO_CHECK(matched("YES|NO", "NO"));
    EO_CHECK(!matched("YES|NO", "YESNO"));
    EO_CHECK(matched("(ab|cd){2}", "abcd"));
    EO_CHECK(matched("(ab|cd){2}", "cdcd"));
    EO_CHECK(!matched("(ab|cd){2}", "ab"));
    EO_CHECK(matched("x(ab|cd)y", "xcdy"));
    EO_CHECK(matched("(|-)[0-9]", "5"));
    EO_CHECK(matched("(|-)[0-9]", "-5"));
    EO_CHECK(matched("a|", ""));
    EO_CHECK(matched("((a|b)c)+", "acbcac"));
    EO_CHECK(!matched("((a|b)c)+", "acb"));
    EO_CHECK(matched("(a(b(c)?)?)*", "aababcab"));
    EO_CHECK(matched("(ab){2,3}", "ababab"));
    EO_CHECK(!matched("(ab){2,3}", "abababab"));
    EO_CHECK(matched("(ab){2,}", "abababab"));
    EO_CHECK(matched("()", ""));
    EO_CHECK(matched("()*a", "a"));
    EO_CHECK(matched("(a?)*b", "aab"));
}

EO_TEST(a_pattern_matches_as_a_regular_expression_does_and_gives_characters_back) {
    EO_CHECK(matched("[a-z]*a", "ba"));
    EO_CHECK(matched("[a-z]{1,10}[a-z]", "abc"));
    EO_CHECK(matched("a*a*a*b", "aaab"));
    EO_CHECK(matched("(a|ab)(c|bcd)", "abcd"));
    EO_CHECK(matched("[a-z]{2,4}[a-c]{2}", "zzzab"));
    EO_CHECK(!matched("[a-z]{2,4}[a-c]{2}", "zzzzzab"));
    EO_CHECK(matched("(x[a-z]{1,3})*", "xaxbbbxcc"));
    EO_CHECK(!matched("(x[a-z]{1,3})*", "xaxbbbbxcc"));
    EO_CHECK(matched("([ab]{2}c)+", "abcbbcaac"));
    EO_CHECK(matched("(a{2,3}){2}", "aaaaa"));
    EO_CHECK(!matched("(a{2,3}){2}", "aaaaaaa"));
}

namespace {

struct compiled {
    eo::detail::pattern_tree tree;
    eo::detail::pattern_program program;

    explicit compiled(char const* text) {
        eo::detail::pattern_parser<eo::detail::pattern_tree> parser(text, tree);
        int const root = parser.whole();
        program = eo::detail::pattern_program(tree, root);
    }

    std::uint64_t visits_on(std::string const& token) const {
        std::uint64_t const before = program.visits();
        program.matches(token);
        return program.visits() - before;
    }
};

}  // namespace

EO_TEST(a_pattern_matches_in_steps_linear_in_the_token) {
    for (char const* text : {"(a*)*b", "(a|a)*b", "(a|aa)*c", "[a-z]*[a-z]{1000}", "(a[a-z]{1,1000})+", "(a?){30}a{30}",
                             "([a-z]+ ?){1,30}"}) {
        compiled const made(text);
        std::uint64_t const short_run = made.visits_on(std::string(10000, 'a'));
        std::uint64_t const long_run = made.visits_on(std::string(20000, 'a'));
        EO_CHECK(long_run * 100 <= short_run * 201 + 409600);
        EO_CHECK(long_run <= 20000ull * 4096);
    }
    std::string const many(200000, 'a');
    EO_CHECK(!matched("(a*)*b", many));
    EO_CHECK(matched("(a?){30}a{30}", std::string(30, 'a')));
    EO_CHECK(matched("[a-z]{200000}", many));
    EO_CHECK(!matched("[a-z]{199999}", many));
    EO_CHECK(matched("[a-z]{1,1000000000}", many));
    EO_CHECK(matched("(a[a-z]{1,1000000})+", many));
    EO_CHECK(matched("[a-z]*[a-z]{100000}", many));
}

EO_TEST(a_pattern_holds_a_bounded_queue_whatever_the_token) {
    std::string const many(100000, 'a');
    compiled const endless("(a*)*");
    EO_CHECK(endless.program.matches(many));
    EO_CHECK(endless.program.entries_held() <= 2);
    compiled const words("([a-z]+ ?){1,30}");
    EO_CHECK(words.program.matches(many));
    EO_CHECK(words.program.entries_held() <= 60);
    compiled const bounded("(a{0,100})*b");
    EO_CHECK(!bounded.program.matches(many));
    EO_CHECK(bounded.program.entries_held() <= 400);
    compiled const counted("[a-z]{1,100000}a");
    EO_CHECK(counted.program.matches(many));
    EO_CHECK(counted.program.entries_held() <= 200);
}

EO_TEST(a_repeat_of_a_group_that_matches_nothing_is_built_at_once) {
    EO_CHECK(matched("(){1000000000}", ""));
    EO_CHECK(!matched("(){1000000000}", "a"));
    EO_CHECK(matched("((){1000000000}){1000000000}x", "x"));
    EO_CHECK(matched("(()*){0,1000000000}x(|){7,}", "x"));
    EO_CHECK_EQ(refusal_of("(a{0}){5000}"), std::string("eolymp.h: tests/pattern.inc:4: eo::pattern(\"(a{0}){5000}\") "
                                                        "needs more than 4096 steps to match; repeat a character or a "
                                                        "class, which costs one step at any count, rather than a group"));
}

EO_TEST(a_pattern_that_does_not_parse_is_refused_at_its_column) {
    char const* const prefix = "eolymp.h: tests/pattern.inc:4: eo::pattern(\"";
    std::string const head = std::string(prefix);
    EO_CHECK_EQ(refusal_of("[a-z"), head + "[a-z\") does not parse at column 1: the [ that opens here has no ] "
                                          "to close it");
    EO_CHECK_EQ(refusal_of("ab[]"), head + "ab[]\") does not parse at column 3: a class needs at least one "
                                          "character; write \\] for a ] inside it");
    EO_CHECK_EQ(refusal_of("[^]"), head + "[^]\") does not parse at column 1: a class needs at least one "
                                         "character; write \\] for a ] inside it");
    EO_CHECK_EQ(refusal_of("x[z-a]"), head + "x[z-a]\") does not parse at column 3: this range runs backwards; "
                                            "write its low end first");
    EO_CHECK_EQ(refusal_of("(ab"), head + "(ab\") does not parse at column 1: the ( that opens here has no ) to "
                                         "close it");
    EO_CHECK_EQ(refusal_of("ab)"), head + "ab)\") does not parse at column 3: this ) closes no group; put a "
                                         "backslash before it for the character");
    EO_CHECK_EQ(refusal_of("a]"), head + "a]\") does not parse at column 2: this ] closes nothing; put a "
                                        "backslash before it for the character");
    EO_CHECK_EQ(refusal_of("a}"), head + "a}\") does not parse at column 2: this } closes nothing; put a "
                                        "backslash before it for the character");
    EO_CHECK_EQ(refusal_of("*a"), head + "*a\") does not parse at column 1: this repeat has nothing before it to "
                                        "repeat; put a backslash before it for the character");
    EO_CHECK_EQ(refusal_of("a|+"), head + "a|+\") does not parse at column 3: this repeat has nothing before it "
                                         "to repeat; put a backslash before it for the character");
    EO_CHECK_EQ(refusal_of("({2})"), head + "({2})\") does not parse at column 2: this repeat has nothing before "
                                           "it to repeat; put a backslash before it for the character");
    EO_CHECK_EQ(refusal_of("a{2}{3}"), head + "a{2}{3}\") does not parse at column 5: a repeat follows a repeat; "
                                             "group the first, as in (a{2}){3}");
    EO_CHECK_EQ(refusal_of("a+?"), head + "a+?\") does not parse at column 3: a repeat follows a repeat; group "
                                         "the first, as in (a{2}){3}");
    for (char const* broken : {"a{", "a{x}", "a{,2}", "a{2", "a{2,x}", "a{2,3", "a{}"})
        EO_CHECK_EQ(refusal_of(broken), head + broken + "\") does not parse at column 2: a count is {n}, {n,m} or "
                                                        "{n,}, with n and m written in digits");
    EO_CHECK_EQ(refusal_of("a{1000000001}"), head + "a{1000000001}\") does not parse at column 2: a count is at "
                                                   "most 1000000000");
    EO_CHECK_EQ(refusal_of("a{0,1000000001}"), head + "a{0,1000000001}\") does not parse at column 2: a count is "
                                                     "at most 1000000000");
    EO_CHECK_EQ(refusal_of("a{3,2}"), head + "a{3,2}\") does not parse at column 2: this count's least is above "
                                            "its most");
    EO_CHECK_EQ(refusal_of("ab\\"), head + "ab\\\") does not parse at column 3: the pattern ends in a "
                                          "backslash with nothing after it to quote");
    EO_CHECK_EQ(refusal_of("[a\\"), head + "[a\\\") does not parse at column 3: the pattern ends in a "
                                          "backslash with nothing after it to quote");
    for (char const* letter : {"\\d", "\\w", "\\s", "\\n", "\\1", "[\\d]"})
        EO_CHECK_IN(refusal_of(letter), "a backslash quotes a symbol, and before a letter or a digit it means "
                                        "nothing here; write the character alone, or a class such as [0-9]");
    EO_CHECK_EQ(refusal_of("^[a-z]$"), head + "^[a-z]$\") does not parse at column 1: a pattern matches the whole "
                                             "token, so it needs no ^ or $; put a backslash before it for the "
                                             "character");
    EO_CHECK_EQ(refusal_of("[a-z]$"), head + "[a-z]$\") does not parse at column 6: a pattern matches the whole "
                                            "token, so it needs no ^ or $; put a backslash before it for the "
                                            "character");
    EO_CHECK_EQ(refusal_of(std::string(51, '(') + std::string(51, ')')),
                head + std::string(51, '(') + std::string(51, ')') +
                    "\") does not parse at column 51: groups nest more than 50 deep");
    EO_CHECK_EQ(refusal_of(std::string(50, '(') + std::string(50, ')')), std::string("built"));
    EO_CHECK_EQ(refusal_of("\x01[a"), head + "\\x01[a\") does not parse at column 2: the [ that opens here has no ] "
                                            "to close it");
}

EO_TEST(a_pattern_too_large_to_match_quickly_is_refused) {
    EO_CHECK_EQ(refusal_of("(ab){3000}"),
                "eolymp.h: tests/pattern.inc:4: eo::pattern(\"(ab){3000}\") needs more than 4096 steps to "
                "match; repeat a character or a class, which costs one step at any count, rather than a group");
    EO_CHECK_IN(refusal_of("((ab){1000}){1000}"), "needs more than 4096 steps");
    EO_CHECK_IN(refusal_of("((((a|b){1000000000}){1000000000}){1000000000}){1000000000}"),
                "needs more than 4096 steps");
    EO_CHECK_EQ(refusal_of("(ab){2000}"), std::string("built"));
    EO_CHECK_IN(refusal_of("(a|b){1000000000}"), "needs more than 4096 steps");
    EO_CHECK_EQ(refusal_of("[ab]{1000000000}"), std::string("built"));
}

EO_TEST(a_pattern_is_refused_where_it_is_made) {
    eot::outcome const made = eot::run_until_stop([] { eo::pattern refused(eo::detail::pattern_text("(", "here.cpp", 7)); });
    EO_CHECK_EQ(made.code, 3);
    EO_CHECK_EQ(made.text, std::string("eolymp.h: here.cpp:7: eo::pattern(\"(\") does not parse at column 1: the ( "
                                       "that opens here has no ) to close it"));
}
