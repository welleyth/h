namespace {

eo::detail::site const somewhere{"validator.cpp", 14};
eo::detail::site const elsewhere{"validator.cpp", 31};

eo::detail::diagnostics& diagnostics() { return eo::detail::diagnostics::shared(); }

struct clean_slate {
    clean_slate() { diagnostics().forget_everything(); }
    ~clean_slate() { diagnostics().forget_everything(); }
};

void raise_warning(eo::detail::site where = somewhere, char const* code = "EO101") {
    eo::detail::warn(code, "read_int(1, n) has no name", "name it: read_int(1, n, \"len\")", where);
}

}  // namespace

EO_TEST(a_site_reads_as_a_file_and_a_line) {
    EO_CHECK_EQ(eo::detail::where_of(somewhere), std::string("validator.cpp:14"));
}

EO_TEST(the_same_call_site_is_counted_not_repeated) {
    clean_slate fresh;
    raise_warning();
    raise_warning();
    raise_warning(elsewhere);
    EO_CHECK_EQ(diagnostics().all().size(), static_cast<std::size_t>(2));
    EO_CHECK_EQ(diagnostics().all()[0].count, 2LL);
    EO_CHECK_EQ(diagnostics().all()[1].count, 1LL);
    EO_CHECK(diagnostics().anything());
}

EO_TEST(a_local_report_names_the_line_the_code_and_the_fix) {
    clean_slate fresh;
    raise_warning();
    raise_warning();
    eo::detail::note("EO304", "cases is used with no sum_limit", "add eo::sum_limit", elsewhere);
    std::string const block = diagnostics().local_block();
    EO_CHECK_IN(block, "validator.cpp:14: warning EO101: read_int(1, n) has no name (2 times)");
    EO_CHECK_IN(block, "  name it: read_int(1, n, \"len\")");
    EO_CHECK_IN(block, "validator.cpp:31: note EO304: cases is used with no sum_limit");
    EO_CHECK(block.find("warning EO101") < block.find("note EO304"));
}

EO_TEST(a_judge_report_is_one_line_each_and_a_machine_line) {
    clean_slate fresh;
    raise_warning();
    eo::detail::note("EO304", "no sum_limit", "add one", elsewhere);
    EO_CHECK_EQ(diagnostics().judge_lines(),
                std::string("warning EO101 validator.cpp:14 read_int(1, n) has no name\n"
                            "note EO304 validator.cpp:31 no sum_limit\n"));
    EO_CHECK_EQ(diagnostics().report_line(),
                std::string("eo-report {\"version\":1,\"warnings\":["
                            "{\"code\":\"EO101\",\"at\":\"validator.cpp:14\",\"count\":1},"
                            "{\"code\":\"EO304\",\"at\":\"validator.cpp:31\",\"count\":1}]}\n"));
}

EO_TEST(a_json_string_escapes_quotes_backslashes_and_control_characters) {
    EO_CHECK_EQ(eo::detail::json_string("checker.cpp:4"), std::string("\"checker.cpp:4\""));
    EO_CHECK_EQ(eo::detail::json_string("a\"b\\c"), std::string("\"a\\\"b\\\\c\""));
    EO_CHECK_EQ(eo::detail::json_string(std::string("\t\n\x1f\x7f\xc3\xa9", 6)),
                std::string("\"\\u0009\\u000a\\u001f\x7f\xc3\xa9\""));
}

EO_TEST(a_long_report_is_capped_and_says_how_many_are_left) {
    clean_slate fresh;
    for (int line = 1; line <= 35; line++) raise_warning(eo::detail::site{"validator.cpp", line});
    EO_CHECK_EQ(diagnostics().all().size(), static_cast<std::size_t>(35));
    std::string const block = diagnostics().local_block();
    EO_CHECK_IN(block, "... and 5 more");
    EO_CHECK_IN(diagnostics().judge_lines(), "... and 5 more");
    EO_CHECK(!eot::contains(block, "validator.cpp:31:"));
}

EO_TEST(a_long_report_lists_warnings_before_notes_in_the_order_raised) {
    clean_slate fresh;
    for (int line = 1; line <= 40; line++) {
        eo::detail::site const where{"validator.cpp", line};
        if (line % 2 == 1) eo::detail::note("EO106", eo::fmt("n{}", line), "fix", where);
        else eo::detail::warn("EO101", eo::fmt("w{}", line), "fix", where);
    }
    std::string wanted;
    for (int line = 2; line <= 40; line += 2) wanted += eo::fmt("warning EO101 validator.cpp:{} w{}\n", line, line);
    for (int line = 1; line <= 19; line += 2) wanted += eo::fmt("note EO106 validator.cpp:{} n{}\n", line, line);
    EO_CHECK_EQ(diagnostics().judge_lines(), wanted + "... and 10 more\n");
}

EO_TEST(a_silenced_code_is_counted_and_listed_with_its_reason) {
    clean_slate fresh;
    {
        eo::allow quiet("EO101", "the length is checked two lines below");
        raise_warning();
        raise_warning();
        EO_CHECK_EQ(diagnostics().all().size(), static_cast<std::size_t>(0));
    }
    raise_warning();
    EO_CHECK_EQ(diagnostics().all().size(), static_cast<std::size_t>(1));
    std::string const block = diagnostics().local_block();
    EO_CHECK_IN(block, "silenced EO101 (2 times): the length is checked two lines below");
}

EO_TEST(a_silence_nobody_used_is_not_listed) {
    clean_slate fresh;
    { eo::allow quiet("EO999", "nothing raises this"); }
    EO_CHECK(diagnostics().anything());
    EO_CHECK(!eot::contains(diagnostics().local_block(), "EO999"));
}

EO_TEST(a_silence_without_a_reason_is_a_library_error) {
    clean_slate fresh;
    eot::outcome const result = eot::run_until_stop([] { eo::allow quiet("EO101", ""); });
    EO_CHECK(result.stopped);
    EO_CHECK_IN(result.text, "eo::allow(\"EO101\") needs a reason");
}

EO_TEST(strict_mode_stops_at_a_warning_but_not_at_a_note) {
    clean_slate fresh;
    ::setenv("EOLYMP_STRICT", "1", 1);
    eot::outcome const stopped = eot::run_until_stop([] { raise_warning(); });
    EO_CHECK(stopped.stopped);
    EO_CHECK_EQ(stopped.code, 3);
    EO_CHECK_IN(stopped.text, "strict mode stops at EO101");
    diagnostics().forget_everything();
    eot::outcome const kept_going =
        eot::run_until_stop([] { eo::detail::note("EO304", "no sum_limit", "add one", elsewhere); });
    EO_CHECK(!kept_going.stopped);
    ::unsetenv("EOLYMP_STRICT");
}

EO_TEST(warnings_reach_the_log_on_the_judge_and_stderr_at_home) {
    clean_slate fresh;
    ::setenv("EOLYMP", "1", 1);
    eot::redirect log;
    raise_warning();
    diagnostics().emit();
    diagnostics().emit();
    std::string const written = log.finish();
    ::unsetenv("EOLYMP");
    EO_CHECK_IN(written, "warning EO101 validator.cpp:14");
    EO_CHECK_IN(written, "eo-report {");
    EO_CHECK_EQ(written.find("warning EO101"), written.rfind("warning EO101"));

    diagnostics().forget_everything();
    eot::redirect complaints(2);
    raise_warning();
    diagnostics().emit();
    std::string const local = complaints.finish();
    EO_CHECK_IN(local, "validator.cpp:14: warning EO101");
}

EO_TEST(a_verdict_carries_the_warnings_with_it) {
    clean_slate fresh;
    eot::outcome const result = eot::run_until_stop([] {
        raise_warning();
        eo::detail::finish(3, "line 1: n is 0");
    });
    EO_CHECK(result.stopped);
    EO_CHECK_EQ(result.printed, std::string("line 1: n is 0\n"));
    EO_CHECK_IN(result.complained, "warning EO101");
}

EO_TEST(a_quiet_run_reports_nothing) {
    clean_slate fresh;
    EO_CHECK(!diagnostics().anything());
    eot::redirect complaints(2);
    diagnostics().emit();
    EO_CHECK_EQ(complaints.finish(), std::string(""));
}
