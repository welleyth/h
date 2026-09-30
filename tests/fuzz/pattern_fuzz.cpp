#include "fuzz.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <regex>

namespace {

char const alphabet[] = {'a', 'b', 'c', '-', '.', ']', '^'};

enum class kind { one, row, either, again };

struct node {
    kind shape = kind::one;
    std::string set;
    bool negated = false;
    bool literal = false;
    std::vector<node> parts;
    int least = 1;
    int most = 1;
    bool nullable = false;
};

std::string hex(char one) {
    char const digits[] = "0123456789abcdef";
    unsigned const byte = static_cast<unsigned char>(one);
    return std::string("\\x") + digits[byte >> 4] + digits[byte & 15];
}

std::string quoted(char one, char const* special) {
    return std::strchr(special, one) != nullptr ? std::string("\\") + one : std::string(1, one);
}

node made_of(FuzzedDataProvider& fdp, int depth);

node row_of(FuzzedDataProvider& fdp, int depth) {
    node out;
    out.shape = kind::row;
    out.nullable = true;
    int const parts = fdp.ConsumeIntegralInRange<int>(0, 3);
    for (int at = 0; at < parts; at++) {
        out.parts.push_back(made_of(fdp, depth + 1));
        out.nullable = out.nullable && out.parts.back().nullable;
    }
    return out;
}

node made_of(FuzzedDataProvider& fdp, int depth) {
    node out;
    int const which = fdp.ConsumeIntegralInRange<int>(0, depth >= 3 ? 1 : 3);
    if (which == 0) {
        out.set = std::string(2, fdp.PickValueInArray(alphabet));
        out.literal = true;
    } else if (which == 1) {
        out.negated = fdp.ConsumeBool();
        int const items = fdp.ConsumeIntegralInRange<int>(1, 3);
        for (int at = 0; at < items; at++) {
            char low = fdp.PickValueInArray(alphabet);
            char high = fdp.PickValueInArray(alphabet);
            if (low > high) std::swap(low, high);
            if (!fdp.ConsumeBool()) high = low;
            out.set += low;
            out.set += high;
        }
    } else {
        out.shape = kind::either;
        int const branches = fdp.ConsumeIntegralInRange<int>(1, 3);
        for (int at = 0; at < branches; at++) {
            out.parts.push_back(row_of(fdp, depth));
            out.nullable = out.nullable || out.parts.back().nullable;
        }
    }
    if (which != 3 && !fdp.ConsumeBool()) return out;
    node repeated;
    repeated.shape = kind::again;
    repeated.least = fdp.ConsumeIntegralInRange<int>(0, 3);
    repeated.most = fdp.ConsumeBool() ? -1 : repeated.least + fdp.ConsumeIntegralInRange<int>(0, 2);
    repeated.nullable = out.nullable || repeated.least == 0;
    repeated.parts.push_back(std::move(out));
    return repeated;
}

bool heavy_for_backtracking(node const& one) {
    if (one.shape == kind::again && one.parts[0].nullable && one.most != 1) return true;
    for (node const& part : one.parts)
        if (heavy_for_backtracking(part)) return true;
    return false;
}

void render(node const& one, std::string& ours, std::string& theirs) {
    if (one.literal) {
        ours += quoted(one.set[0], "\\[](){}|?*+^$");
        theirs += hex(one.set[0]);
    } else if (one.shape == kind::one) {
        ours += one.negated ? "[^" : "[";
        theirs += one.negated ? "[^" : "[";
        for (std::size_t at = 0; at < one.set.size(); at += 2) {
            ours += quoted(one.set[at], "\\]^-[");
            theirs += hex(one.set[at]);
            if (one.set[at + 1] == one.set[at]) continue;
            ours += "-" + quoted(one.set[at + 1], "\\]^-[");
            theirs += "-" + hex(one.set[at + 1]);
        }
        ours += "]";
        theirs += "]";
    } else if (one.shape == kind::row) {
        for (node const& part : one.parts) render(part, ours, theirs);
    } else if (one.shape == kind::either) {
        ours += "(";
        theirs += "(?:";
        for (std::size_t at = 0; at < one.parts.size(); at++) {
            if (at > 0) {
                ours += "|";
                theirs += "|";
            }
            render(one.parts[at], ours, theirs);
        }
        ours += ")";
        theirs += ")";
    } else {
        render(one.parts[0], ours, theirs);
        std::string times;
        if (one.least == 0 && one.most == 1) times = "?";
        else if (one.least == 0 && one.most < 0) times = "*";
        else if (one.least == 1 && one.most < 0) times = "+";
        else if (one.most < 0) times = "{" + std::to_string(one.least) + ",}";
        else if (one.least == one.most) times = "{" + std::to_string(one.least) + "}";
        else times = "{" + std::to_string(one.least) + "," + std::to_string(one.most) + "}";
        ours += times;
        theirs += times;
    }
}

using ends = std::uint32_t;

bool holds(node const& one, char c) {
    bool inside = false;
    for (std::size_t at = 0; at < one.set.size(); at += 2)
        inside = inside || (c >= one.set[at] && c <= one.set[at + 1]);
    return inside != one.negated;
}

ends reached(node const& one, std::string const& token, ends from) {
    if (one.shape == kind::one) {
        ends out = 0;
        for (std::size_t start = 0; start < token.size(); start++)
            if ((from >> start & 1) != 0 && holds(one, token[start])) out |= ends{1} << (start + 1);
        return out;
    }
    if (one.shape == kind::row) {
        for (node const& part : one.parts) from = reached(part, token, from);
        return from;
    }
    if (one.shape == kind::either) {
        ends out = 0;
        for (node const& part : one.parts) out |= reached(part, token, from);
        return out;
    }
    ends now = from;
    for (int copy = 0; copy < one.least; copy++) now = reached(one.parts[0], token, now);
    ends out = now;
    if (one.most < 0) {
        for (ends grown = out | reached(one.parts[0], token, out); grown != out;
             grown = out | reached(one.parts[0], token, out))
            out = grown;
        return out;
    }
    for (int copy = one.least; copy < one.most; copy++) {
        now = reached(one.parts[0], token, now);
        out |= now;
    }
    return out;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size) {
    FuzzedDataProvider fdp(data, size);
    if (fdp.ConsumeBool()) {
        std::string const text = fdp.ConsumeRandomLengthString(64);
        std::string const token = fdp.ConsumeRemainingBytesAsString();
        eof::verdict const made = eof::run([&] { (void)eo::pattern(text).matches(token); });
        eof::must(!made.stopped || made.code == 3, "a pattern is either built or refused");
        eof::must(!made.stopped || made.text.find("eo::pattern(\"") != std::string::npos,
                  "a refused pattern names itself");
        return 0;
    }
    node const tree = row_of(fdp, 0);
    std::string ours;
    std::string theirs;
    render(tree, ours, theirs);
    bool const backtracks = !heavy_for_backtracking(tree);
    std::regex const reference(backtracks ? theirs : std::string(), std::regex::ECMAScript);
    eo::pattern const pattern(ours);
    for (int round = 0; round < 8 && fdp.remaining_bytes() > 0; round++) {
        std::string token;
        int const length = fdp.ConsumeIntegralInRange<int>(0, 7);
        for (int at = 0; at < length; at++) token += fdp.PickValueInArray(alphabet);
        bool const found = pattern.matches(token);
        bool const by_positions = (reached(tree, token, 1) >> token.size() & 1) != 0;
        bool const by_regex = backtracks ? std::regex_match(token, reference) : by_positions;
        if (found != by_positions || found != by_regex) {
            ::dprintf(eof::loud(), "pattern %s (as %s) on \"%s\": %d, by positions %d, by std::regex %d\n",
                      ours.c_str(), theirs.c_str(), token.c_str(), found, by_positions, by_regex);
            eof::must(false, "a pattern matches what std::regex and a set of end positions match");
        }
    }
    return 0;
}
