EO_TEST(version_is_the_macro) {
    EO_CHECK_EQ(std::string(eo::version()), std::string(EOLYMP_H_VERSION));
}

EO_TEST(version_parts_are_the_numbers_in_the_version) {
    static_assert(eo::detail::version_part("12.34.56-rc1", 0) == 12);
    static_assert(eo::detail::version_part("12.34.56-rc1", 1) == 34);
    static_assert(eo::detail::version_part("12.34.56-rc1", 2) == 56);
    static_assert(eo::detail::version_part("1.0.10", 2) == 10);
    char const* const running = eo::version();
    EO_CHECK_EQ(eo::detail::version_part(running, 0), long{EOLYMP_H_VERSION_MAJOR});
    EO_CHECK_EQ(eo::detail::version_part(running, 1), long{EOLYMP_H_VERSION_MINOR});
    EO_CHECK_EQ(eo::detail::version_part(running, 2), long{EOLYMP_H_VERSION_PATCH});
    std::string const joined = std::to_string(EOLYMP_H_VERSION_MAJOR) + "." + std::to_string(EOLYMP_H_VERSION_MINOR) +
                               "." + std::to_string(EOLYMP_H_VERSION_PATCH);
    EO_CHECK_EQ(std::string(EOLYMP_H_VERSION).substr(0, joined.size()), joined);
#if EOLYMP_H_VERSION_MAJOR < 1
    EO_CHECK(false);
#endif
}

EO_TEST(environment_reads_the_process_environment) {
    ::unsetenv("EOLYMP_TEST_KNOB");
    EO_CHECK(eo::detail::environment("EOLYMP_TEST_KNOB") == nullptr);
    EO_CHECK(!eo::detail::environment_is("EOLYMP_TEST_KNOB", "1"));
    ::setenv("EOLYMP_TEST_KNOB", "2", 1);
    EO_CHECK(!eo::detail::environment_is("EOLYMP_TEST_KNOB", "1"));
    ::setenv("EOLYMP_TEST_KNOB", "1", 1);
    EO_CHECK(eo::detail::environment_is("EOLYMP_TEST_KNOB", "1"));
    ::unsetenv("EOLYMP_TEST_KNOB");
}

EO_TEST(judge_and_strict_come_from_the_environment) {
    ::unsetenv("EOLYMP");
    ::unsetenv("EOLYMP_STRICT");
    EO_CHECK(!eo::detail::on_judge());
    EO_CHECK(!eo::detail::strict_mode());
    ::setenv("EOLYMP", "1", 1);
    ::setenv("EOLYMP_STRICT", "1", 1);
    EO_CHECK(eo::detail::on_judge());
    EO_CHECK(eo::detail::strict_mode());
    ::unsetenv("EOLYMP");
    ::unsetenv("EOLYMP_STRICT");
}

EO_TEST(report_ends_every_message_with_one_line_break) {
    eot::redirect capture;
    eo::detail::report("");
    eo::detail::report("no break");
    eo::detail::report("has break\n");
    EO_CHECK_EQ(capture.finish(), std::string("no break\nhas break\n"));
}

EO_TEST(finish_carries_the_code_and_prints_the_text) {
    eot::outcome const result = eot::run_until_stop([] { eo::detail::finish(3, "invalid test"); });
    EO_CHECK(result.stopped);
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_EQ(result.text, std::string("invalid test"));
    EO_CHECK_EQ(result.printed, std::string("invalid test\n"));
}

EO_TEST(library_error_names_the_library) {
    eot::outcome const result = eot::run_until_stop([] { eo::detail::library_error("two validators"); });
    EO_CHECK(result.stopped);
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_EQ(result.text, std::string("eolymp.h: two validators"));
}

EO_TEST(any_and_unnamed_are_distinct_markers) {
    EO_CHECK(!std::is_same_v<eo::any_t, eo::unnamed_t>);
    EO_CHECK(std::is_same_v<decltype(eo::any), eo::any_t const>);
    EO_CHECK(std::is_same_v<decltype(eo::unnamed), eo::unnamed_t const>);
}

EO_TEST(checked_arithmetic_by_hand_agrees_with_the_builtins) {
    long long const big = std::numeric_limits<long long>::max();
    long long const small = std::numeric_limits<long long>::min();
    std::vector<long long> values = {small, small + 1, small + 2, small / 2, -3037000500LL, -3037000499LL,
                                     -4294967296LL, -2147483648LL, -3, -2, -1, 0, 1, 2, 3, 2147483647LL,
                                     4294967296LL, 3037000499LL, 3037000500LL, big / 2, big / 2 + 1, big - 2,
                                     big - 1, big};
    std::uint64_t state = 0x2545f4914f6cdd1dULL;
    for (int drawn = 0; drawn < 2000; drawn++) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        values.push_back(static_cast<long long>(state >> (drawn % 63)) * (drawn % 2 == 0 ? 1 : -1));
    }
    long long disagreements = 0;
    for (long long const left : values)
        for (long long const right : values) {
            long long by_hand = 0;
            long long built_in = 0;
            if (eo::detail::sum_overflows_by_hand(left, right, &by_hand) !=
                    __builtin_add_overflow(left, right, &built_in) ||
                by_hand != built_in)
                disagreements++;
            if (eo::detail::difference_overflows_by_hand(left, right, &by_hand) !=
                    __builtin_sub_overflow(left, right, &built_in) ||
                by_hand != built_in)
                disagreements++;
            if (eo::detail::product_overflows_by_hand(left, right, &by_hand) !=
                    __builtin_mul_overflow(left, right, &built_in) ||
                by_hand != built_in)
                disagreements++;
            if (eo::detail::sum_overflows(left, right, &by_hand) != __builtin_add_overflow(left, right, &built_in) ||
                eo::detail::difference_overflows(left, right, &by_hand) !=
                    __builtin_sub_overflow(left, right, &built_in) ||
                eo::detail::product_overflows(left, right, &by_hand) !=
                    __builtin_mul_overflow(left, right, &built_in))
                disagreements++;
        }
    EO_CHECK_EQ(disagreements, 0LL);
}
