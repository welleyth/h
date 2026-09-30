EO_TEST(fmt_without_values_copies_the_pattern) {
    EO_CHECK_EQ(eo::fmt("plain text"), std::string("plain text"));
    EO_CHECK_EQ(eo::fmt(""), std::string(""));
}

EO_TEST(fmt_substitutes_in_order) {
    EO_CHECK_EQ(eo::fmt("{} and {}", 1, 2), std::string("1 and 2"));
    EO_CHECK_EQ(eo::fmt("a[{}]", 5), std::string("a[5]"));
}

EO_TEST(fmt_escapes_braces) {
    EO_CHECK_EQ(eo::fmt("{{}}"), std::string("{}"));
    EO_CHECK_EQ(eo::fmt("{{{}}}", 7), std::string("{7}"));
}

EO_TEST(fmt_writes_integers_of_every_width) {
    EO_CHECK_EQ(eo::fmt("{}", 0), std::string("0"));
    EO_CHECK_EQ(eo::fmt("{}", -12), std::string("-12"));
    EO_CHECK_EQ(eo::fmt("{}", static_cast<short>(-3)), std::string("-3"));
    EO_CHECK_EQ(eo::fmt("{}", 9223372036854775807LL), std::string("9223372036854775807"));
    EO_CHECK_EQ(eo::fmt("{}", -9223372036854775807LL - 1), std::string("-9223372036854775808"));
    EO_CHECK_EQ(eo::fmt("{}", 18446744073709551615ULL), std::string("18446744073709551615"));
    EO_CHECK_EQ(eo::fmt("{}", static_cast<unsigned>(7)), std::string("7"));
    EO_CHECK_EQ(eo::fmt("{}", static_cast<std::size_t>(42)), std::string("42"));
}

EO_TEST(fmt_writes_reals_that_read_back_exactly) {
    EO_CHECK_EQ(eo::fmt("{}", 0.5), std::string("0.5"));
    EO_CHECK_EQ(eo::fmt("{}", 1.0f), std::string("1"));
    EO_CHECK_EQ(eo::fmt("{}", -0.25), std::string("-0.25"));
    double const awkward = 1.0 + 2.220446049250313e-16;
    std::string const written = eo::fmt("{}", awkward);
    EO_CHECK(std::strtod(written.c_str(), nullptr) == awkward);
    EO_CHECK(written.size() > 1);
}

EO_TEST(fmt_writes_nan_and_inf_the_same_way_under_every_library) {
    double const nothing = std::nan("");
    double const endless = std::numeric_limits<double>::infinity();
    EO_CHECK_EQ(eo::fmt("{}", nothing), std::string("nan"));
    EO_CHECK_EQ(eo::fmt("{}", -nothing), std::string("-nan"));
    EO_CHECK_EQ(eo::fmt("{}", endless), std::string("inf"));
    EO_CHECK_EQ(eo::fmt("{}", -endless), std::string("-inf"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(nothing, 6)), std::string("nan"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(-nothing, 6)), std::string("-nan"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(endless, 2)), std::string("inf"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(-endless, 2)), std::string("-inf"));
}

EO_TEST(fmt_writes_a_fixed_number_of_any_length) {
    EO_CHECK_EQ(eo::fmt("[{}]", eo::fixed(2.5, 3)), std::string("[2.500]"));
    std::string const huge = eo::fmt("[{}]", eo::fixed(1e300, 2));
    EO_CHECK_EQ(huge.size(), static_cast<std::size_t>(306));
    EO_CHECK_EQ(huge.substr(0, 3), std::string("[10"));
    EO_CHECK_EQ(huge.substr(huge.size() - 4), std::string(".00]"));
    std::string const digits = eo::fmt("{}", eo::fixed(0.5, 100));
    EO_CHECK_EQ(digits, "0.5" + std::string(99, '0'));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(0.5, 61)), "0.5" + std::string(60, '0'));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(0.5, 62)), "0.5" + std::string(61, '0'));
}

EO_TEST(fmt_writes_text_of_every_kind) {
    char mutable_text[] = "mutable";
    char* pointer = mutable_text;
    char const* constant = "constant";
    char const* missing = nullptr;
    EO_CHECK_EQ(eo::fmt("{}", "literal"), std::string("literal"));
    EO_CHECK_EQ(eo::fmt("{}", constant), std::string("constant"));
    EO_CHECK_EQ(eo::fmt("{}", pointer), std::string("mutable"));
    EO_CHECK_EQ(eo::fmt("{}", missing), std::string("(null)"));
    EO_CHECK_EQ(eo::fmt("{}", std::string("owned")), std::string("owned"));
    EO_CHECK_EQ(eo::fmt("{}", std::string_view("viewed")), std::string("viewed"));
    EO_CHECK_EQ(eo::fmt("{}", 'c'), std::string("c"));
    EO_CHECK_EQ(eo::fmt("{} {}", true, false), std::string("true false"));
}

EO_TEST(fmt_keeps_a_lone_brace_and_reports_it) {
    eo::detail::diagnostics::shared().forget_everything();
    EO_CHECK_EQ(eo::fmt("a { b"), std::string("a { b"));
    EO_CHECK_IN(eo::detail::diagnostics::shared().local_block(),
                "warning EO112: the message \"a { b\" has 0 {} for 0 values and a lone '{'");
    eo::detail::diagnostics::shared().forget_everything();
    EO_CHECK_EQ(eo::fmt("a } {} {", 7), std::string("a } 7 {"));
    EO_CHECK_IN(eo::detail::diagnostics::shared().local_block(), "has 1 {} for 1 values and a lone '}'");
    eo::detail::diagnostics::shared().forget_everything();
    EO_CHECK_EQ(eo::fmt("{{}} {}", 1), std::string("{} 1"));
    EO_CHECK(!eo::detail::diagnostics::shared().anything());
}

EO_TEST(the_compile_time_rule_for_a_message_is_the_run_time_rule) {
    static_assert(eo::detail::fault_of("{} and {}", 2) == eo::detail::pattern_fault::none);
    static_assert(eo::detail::fault_of("got %d", 1) == eo::detail::pattern_fault::count);
    static_assert(eo::detail::fault_of("a { b", 0) == eo::detail::pattern_fault::lone);
    struct sample {
        char const* text;
        std::size_t values;
    };
    sample const samples[] = {{"", 0},        {"{}", 1},         {"{}", 0},        {"{{}}", 0},     {"{{}}", 1},
                              {"{{{}}}", 1},  {"a { b", 0},      {"a } {} {", 1},  {"}{", 0},       {"{}}", 1},
                              {"{{}", 1},     {"%d %s", 2},      {"{} {} {}", 2},  {"x}}", 0},      {"{", 0}};
    for (sample const& one : samples) {
        eo::detail::diagnostics::shared().forget_everything();
        std::string const text = one.text;
        if (one.values == 0) {
            (void)eo::fmt(text);
        } else if (one.values == 1) {
            (void)eo::fmt(text, 1);
        } else {
            (void)eo::fmt(text, 1, 2);
        }
        bool const reported = eo::detail::diagnostics::shared().raised_already("EO112");
        bool const refused = eo::detail::fault_of(one.text, one.values) != eo::detail::pattern_fault::none;
        if (reported != refused) std::fprintf(stderr, "   the rules differ on \"%s\"\n", one.text);
        EO_CHECK_EQ(reported, refused);
    }
    eo::detail::diagnostics::shared().forget_everything();
}

EO_TEST(fmt_keeps_every_value_when_the_count_does_not_match) {
    eo::detail::diagnostics::shared().forget_everything();
    int const line = __LINE__ + 1;
    EO_CHECK_EQ(eo::fmt("{} {}", 1), std::string("1 {}"));
    EO_CHECK_IN(eo::detail::diagnostics::shared().local_block(),
                eo::fmt("fmt.inc:{}: warning EO112", line).c_str());
    EO_CHECK_IN(eo::detail::diagnostics::shared().local_block(),
                "warning EO112: the message \"{} {}\" has 2 {} for 1 values");
    eo::detail::diagnostics::shared().forget_everything();
    EO_CHECK_EQ(eo::fmt("{}", 1, 2, "three"), std::string("1 2 three"));
    EO_CHECK_IN(eo::detail::diagnostics::shared().local_block(), "has 1 {} for 3 values");
    eo::detail::diagnostics::shared().forget_everything();
    EO_CHECK_EQ(eo::fmt("expected %lld", 5), std::string("expected %lld 5"));
    eo::detail::diagnostics::shared().forget_everything();
}

EO_TEST(a_number_is_moved_between_the_dot_and_a_locale_s_point) {
    EO_CHECK_EQ(eo::detail::with_the_local_point("1.5", ","), std::string("1,5"));
    EO_CHECK_EQ(eo::detail::with_the_local_point("-0.25e3", "\xd9\xab"), std::string("-0\xd9\xab" "25e3"));
    EO_CHECK_EQ(eo::detail::with_the_local_point("15", ","), std::string("15"));
    EO_CHECK_EQ(eo::detail::with_the_local_point("1.5", "."), std::string("1.5"));
    std::string printed = "1,5 and 2,5";
    eo::detail::with_a_dot(printed, 4, ",");
    EO_CHECK_EQ(printed, std::string("1,5 and 2.5"));
    std::string wide = "x0\xd9\xab" "125";
    eo::detail::with_a_dot(wide, 1, "\xd9\xab");
    EO_CHECK_EQ(wide, std::string("x0.125"));
    std::string plain = "1.5";
    eo::detail::with_a_dot(plain, 0, ".");
    EO_CHECK_EQ(plain, std::string("1.5"));
    std::string whole = "15";
    eo::detail::with_a_dot(whole, 0, ",");
    EO_CHECK_EQ(whole, std::string("15"));
}
