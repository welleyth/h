#!/usr/bin/env python3
"""Every line of both headers runs, which is a weak thing to know.

This changes one operator or bound at a time and requires the suite to notice. A mutant
that survives is a line the tests execute without checking what it is for. The table is
the set an outside review found surviving, plus what has been added since.
"""
import concurrent.futures
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time

from common import ROOT, compiler

alone_limit = 600
least_limit = 60

MUTANTS = [
    ("a sum limit that allows one more", "src/validate.h",
     "if (total_ > limit_)", "if (total_ > limit_ + 1)"),
    ("the examples' row compared when it is listed last", "src/validate.h",
     "rows_[at].group != 0 && rows_[other].group != 0 &&", "rows_[at].group != 0 &&"),
    ("a test whose earlier write failed passed as written", "src/generate.h",
     "        if (std::ferror(stdout))\n            detail::finish(3, \"the test could not be written: an",
     "        if (false)\n            detail::finish(3, \"the test could not be written: an"),
    ("negative points let through down to -1", "src/role.h",
     "    if (paid < 0) {", "    if (paid < -1) {"),
    ("a token one character too long", "src/stream.h",
     "if (length > most)\n                refuse(name,",
     "if (length > most + 1)\n                refuse(name,"),
    ("a line one character too short", "src/stream.h",
     "if (bounds == stated::yes && length < least)", "if (bounds == stated::yes && length < least - 1)"),
    ("a real with one digit too many", "src/stream.h",
     "parsed.decimals > most_decimals", "parsed.decimals > most_decimals + 1"),
    ("bounds whose low end is nearly round remembered as quiet", "src/stream.h",
     "!whole_range && !too_wide && !high_near && !low_near,", "!whole_range && !too_wide && !high_near,"),
    ("a bound one above a value read called nearly round", "src/stream.h",
     "one.second.last_whole >= bound - 1", "one.second.last_whole >= bound"),
    ("a self loop in a simple graph", "src/structure.h",
     "while (loop < edges.size() && edges[loop].u != edges[loop].v) loop++;",
     "while (loop < edges.size()) loop++;"),
    ("a tolerance that excludes its bound", "src/role.h",
     "if (spread <= epsilon + 1e-15) return true;", "if (spread <= epsilon) return true;"),
    ("two infinities that are not equal within a tolerance", "src/check.h",
     "if (found == by_the_jury || close_enough(by_the_jury, found, allowed.epsilon))",
     "if (close_enough(by_the_jury, found, allowed.epsilon))"),
    ("a tolerance relative to the contestant's value", "src/role.h",
     "double const scale = std::fabs(expected);", "double const scale = std::fabs(found);"),
    ("a score that floors instead of rounding", "src/role.h",
     "return std::round(value * scale) / scale;", "return std::floor(value * scale) / scale;"),
    ("an answer whose trailing spaces count", "src/check.h",
     "while (!want.empty() && trailing_blank(want.back())) want.pop_back();", ""),
    ("an extra line that starts with a blank let through", "src/check.h",
     "if (jury_done && (longer || !got.empty()))", "if (jury_done && !got.empty())"),
    ("every random stream the same", "src/generate.h",
     "return label.empty() ? base_ : detail::seed_of(label) * 0x9e3779b97f4a7c15ull ^ base_;",
     "return base_;"),
    ("a flush inside a line that lets a trailing space out", "src/generate.h",
     "if (held_.size() >= detail::mebibyte && trailing_ == 0) flush();",
     "if (held_.size() >= detail::mebibyte) flush();"),
    ("a carriage return counted in a line's length", "src/stream.h",
     "            text.pop_back();\n            seen--;", "            text.pop_back();"),
    ("a token read past the cap it was given", "src/stream.h",
     "            if (cap > 0 && static_cast<long long>(token.size()) >= cap) break;", ""),
    ("a channel read through the default 1 MB buffer", "src/io.h",
     "return over_file(path, false, pipe_chunk);", "return over_file(path, false);"),
    ("a large send that stops taking in the answers", "src/os.h",
     "            absorbed const what = from.absorb(absorb_limit);\n            if (what == absorbed::full) warn",
     "            absorbed const what = absorbed::nothing;\n            if (what == absorbed::full) warn"),
    ("a buffer for the answers of 64 MB", "src/os.h",
     "absorb_limit = std::size_t{1} << 24;", "absorb_limit = std::size_t{1} << 26;"),
    ("a large send that does not listen to the other side", "src/os.h",
     "{listening ? from.listening_descriptor() : -1, POLLIN, 0}", "{-1, POLLIN, 0}"),
    ("a send cut short by a signal taken for a deaf reader", "src/os.h",
     "if (ready < 0 && errno != EINTR && errno != EAGAIN) deaf = true;", "if (ready < 0) deaf = true;"),
    ("a sum by hand that overflows one early", "src/core.h",
     "right > 0 ? left > LLONG_MAX - right", "right > 0 ? left >= LLONG_MAX - right"),
    ("a difference by hand that overflows one early", "src/core.h",
     "right < 0 ? left > LLONG_MAX + right", "right < 0 ? left >= LLONG_MAX + right"),
    ("a product by hand that lets the most negative value times -1 through", "src/core.h",
     "return left != 0 && right < LLONG_MAX / left;",
     "return left != 0 && right < LLONG_MAX / left && right != LLONG_MIN;"),
    ("a look ahead that reads", "src/io.h",
     "        while (held() < limit && top_up()) {\n        }\n",
     "        have(limit);\n"),
    ("a repeat of a class that forgets its entry one character early", "src/pattern.h",
     "after - entries[head] > step.most) head++;", "after - entries[head] >= step.most) head++;"),
    ("a repeat of a class that ends one character late", "src/pattern.h",
     "if (at - entries[head] >= step.least) reach(into, step.next, at);",
     "if (at - entries[head] > step.least) reach(into, step.next, at);"),
    ("a repeat of a class that may be empty and is never skipped", "src/pattern.h",
     "if (step.code == step_code::counted && step.least == 0) pending_.push_back(step.next);", ""),
    ("a class written with ^ read as the class itself", "src/pattern.h",
     "for (std::uint64_t& word : chosen) word = ~word;", ""),
    ("a token that a pattern read lets one character past its longest match", "src/stream.h",
     "if (cap > 0 && static_cast<long long>(token.size()) == cap)",
     "if (cap > 0 && static_cast<long long>(token.size()) > cap)"),
    ("tokens in any case compared in their case", "src/check.h",
     "if (fold ? !detail::same_in_any_case(want, got) : want != got)", "if (want != got)"),
    ("reals within an absolute error that also allow a relative one", "src/check.h",
     "                                           : std::fabs(wanted.value - found.value) <= epsilon + 1e-15;",
     "                                           : close_enough(wanted.value, found.value, epsilon);"),
    ("an output-only checker whose unread answer is still reported", "src/check.h",
     "        output_only_ = true;\n        jury.skip_rest(reason);", "        output_only_ = true;"),
    ("an output-only checker still reported for reading neither file", "src/check.h",
     "!stock_ && !output_only_ &&", "!stock_ &&"),
    ("a salt of 31 hex digits let through", "src/generate.h",
     "if (text.size() != 32)", "if (text.size() > 32)"),
    ("a salt that is checked and then not mixed in", "src/generate.h",
     "base_ = secret == nullptr ? detail::seed_of(all) : detail::siphash(secret->high_, secret->low_, all);",
     "base_ = detail::seed_of(all);"),
    ("a salt's second half thrown away", "src/generate.h",
     "std::uint64_t& word = at < 16 ? high_ : low_;", "std::uint64_t& word = at < 16 ? high_ : high_;"),
    ("SipHash finished with three rounds", "src/random.h",
     "for (int times = 0; times < 4; times++) round();", "for (int times = 0; times < 3; times++) round();"),
    ("a stream that moved taken for one that did not", "src/generate.h",
     "if (one.second.state_ != seed_for(one.first) || one.second.copied_) return true;", ""),
    ("v.features that declares nothing", "src/validate.h",
     "        for (char const* one : names) feature(one);\n", ""),
    ("a stat that keeps the smaller value", "src/validate.h",
     "for (detail::stat_entry& one : stats_)\n            if (one.name == name) {\n                one.value = (std::max)(one.value, value);",
     "for (detail::stat_entry& one : stats_)\n            if (one.name == name) {\n                one.value = (std::min)(one.value, value);"),
    ("a value read twice taken for a size", "src/validate.h",
     "if (seen.element || seen.reads != 1 ||", "if (seen.element || seen.reads < 1 ||"),
    ("an element read once taken for a size", "src/validate.h",
     "if (seen.element || seen.reads != 1 ||", "if (seen.reads != 1 ||"),
    ("a diameter measured from vertex 1", "src/structure.h",
     "shape.diameter = around.farthest_from(deepest.first).second;",
     "shape.diameter = around.farthest_from(1).second;"),
    ("components counted as the edges that join two", "src/structure.h",
     "        if (left != right) components--;", "        components--;"),
    ("a line's length not recorded for its stat", "src/stream.h",
     "            if (name.known()) last_bounds_->last_length = length;\n        }\n        end_the_line(name);",
     "        }\n        end_the_line(name);"),
    ("a token's length not recorded for its stat", "src/stream.h",
     "                     where);\n            if (name.known()) last_bounds_->last_length = length;\n        }\n    }",
     "                     where);\n        }\n    }"),
    ("reads never counted, so every value is a size", "src/stream.h", "        known.reads++;\n", ""),
    ("a sum's total that is no stat", "src/validate.h", "        detail::a_sum_was_checked(name_, total_);\n", ""),
    ("a lone vertex counted as a leaf", "src/structure.h",
     "if (around.degree(vertex) == 1) shape.leaves++;", "if (around.degree(vertex) <= 1) shape.leaves++;"),
    ("a depth measured from the deepest vertex", "src/structure.h",
     "shape.depth = deepest.second;", "shape.depth = around.farthest_from(deepest.first).second;"),
    ("a stat that the value read once overrides", "src/validate.h",
     "        for (auto const& one : stats_) keep_the_larger(all, one.name, one.value);\n",
     "        for (auto const& one : stats_) all.push_back(one);\n"),
    ("a stat's name never checked", "src/validate.h", "            if (!detail::in_a_word(one)) return false;\n", ""),
    ("a space kept in a sum's stat name", "src/validate.h",
     "           one == '.' || one == '-';", "           one == '.' || one == '-' || one == ' ';"),
    ("a copied stream not counted as drawn", "src/random.h",
     "    rng(rng const& other) : state_(other.state_) { other.copied_ = true; }",
     "    rng(rng const& other) : state_(other.state_) {}"),
    ("an assigned stream not counted as drawn", "src/random.h",
     "        state_ = other.state_;\n        other.copied_ = true;", "        state_ = other.state_;"),
    ("a published salt taken for a secret", "src/generate.h",
     "            published_ = published_ || lower == one;", "            published_ = false;"),
    ("a randomness line that does not start a line", "src/generate.h",
     'said += fmt("\\neo-describe randomness', 'said += fmt("eo-describe randomness'),
    ("shape stats not named after their read", "src/validate.h",
     'return detail::one_word(name.known() ? name.key() : std::string(otherwise)) + ".";', 'return std::string();'),
]


def run_a_copy(root, compiler, limit, where=None, old=None, new=None):
    with tempfile.TemporaryDirectory(prefix="eolymp-mutant-") as scratch:
        copy = pathlib.Path(scratch)
        for part in ("src", "tools", "tests"):
            shutil.copytree(root / part, copy / part)
        if where is not None:
            (copy / where).write_text((root / where).read_text().replace(old, new))
        subprocess.run([sys.executable, "tools/amalgamate.py"], cwd=copy, check=True,
                       capture_output=True)
        built = subprocess.run([*compiler, "-std=c++17", "-O0", "-DEOLYMP_TESTING", "-o", "mutant",
                                "tests/all.cpp"], cwd=copy, capture_output=True)
        if built.returncode != 0:
            return "does not compile"
        started = time.monotonic()
        try:
            ran = subprocess.run(["./mutant"], cwd=copy, capture_output=True, timeout=limit)
        except subprocess.TimeoutExpired:
            return "timed out", limit
        return "passed" if ran.returncode == 0 else "failed", time.monotonic() - started


def attempt(root, mutant, compiler, limit):
    name, where, old, new = mutant
    before = (root / where).read_text()
    if before.count(old) != 1:
        return name, f"its anchor appears {before.count(old)} times in {where}"
    outcome, _ = run_a_copy(root, compiler, limit, where, old, new)
    if outcome == "does not compile":
        return name, "the mutant does not compile"
    return name, "survived" if outcome == "passed" else None


def main() -> int:
    root = ROOT
    command = compiler()
    workers = int(os.environ.get("JOBS", os.cpu_count() or 1))
    control, alone = run_a_copy(root, command, alone_limit)
    if control != "passed":
        print(f"mutants: the unchanged sources, copied and built the same way, {control}; "
              f"no mutant can be said to be killed")
        return 1
    limit = max(least_limit, int(alone * workers))
    with concurrent.futures.ThreadPoolExecutor(workers) as pool:
        outcomes = list(pool.map(lambda one: attempt(root, one, command, limit), MUTANTS))
    failed = [(name, why) for name, why in outcomes if why]
    print(f"mutants: {len(MUTANTS) - len(failed)} of {len(MUTANTS)} killed")
    for name, why in failed:
        print(f"  {name}: {why}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
