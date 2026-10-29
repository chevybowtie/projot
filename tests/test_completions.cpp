// Guards against the shell completion scripts drifting from the real CLI.
//
// The source of truth is the valid_flags() table in src/main.cpp. Each script is
// parsed just enough to recover which commands it lists and which flags it offers
// per command, and both are compared with the table in each direction: a missing
// entry means tab completion lacks a feature, an extra one means it offers
// something the CLI would reject.

#include "doctest.h"

#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef PROJOT_SOURCE_DIR
#error "PROJOT_SOURCE_DIR must be defined by the build"
#endif

namespace {

using FlagMap = std::map<std::string, std::set<std::string>>;

struct Parsed {
    std::set<std::string> listed;  // commands offered at the top level
    FlagMap flags;                 // command -> flags offered (with leading --)
};

std::string read_text(const std::string& rel) {
    std::ifstream f(std::string(PROJOT_SOURCE_DIR) + "/" + rel, std::ios::binary);
    REQUIRE_MESSAGE(f.good(), "cannot open " << rel);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();

    // A Windows checkout may convert the scripts to CRLF. Normalise so the parsers can
    // rely on "\n", notably when joining fish's backslash-continued lines.
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue;
        out += text[i];
    }
    return out;
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) out.push_back(trim(line));
    return out;
}

std::vector<std::string> split_on(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep)) out.push_back(item);
    return out;
}

std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (ss >> item) out.push_back(item);
    return out;
}

std::set<std::string> find_flags(const std::string& line) {
    static const std::regex re(R"(--[a-z][a-z-]*)");
    std::set<std::string> out;
    for (std::sregex_iterator it(line.begin(), line.end(), re), end; it != end; ++it)
        out.insert(it->str());
    return out;
}

// True for a shell `case` arm such as `add|add-todo)`.
bool is_case_arm(const std::string& t) {
    static const std::regex re(R"(^[a-z][a-z|-]*\)$)");
    return std::regex_match(t, re);
}

// What the CLI really accepts, read from the valid_flags() table in main.cpp.
FlagMap source_flags() {
    std::string text = read_text("src/main.cpp");
    auto begin = text.find("valid_flags() {");
    REQUIRE(begin != std::string::npos);
    auto end = text.find("return m;", begin);
    REQUIRE(end != std::string::npos);
    std::string block = text.substr(begin, end - begin);

    static const std::regex entry(R"re(\{"([a-z-]+)",\s*\{([^}]*)\}\})re");
    static const std::regex quoted(R"re("([a-z-]+)")re");
    FlagMap out;
    for (std::sregex_iterator it(block.begin(), block.end(), entry), stop; it != stop; ++it) {
        std::string flags = (*it)[2].str();
        auto& set = out[(*it)[1].str()];
        for (std::sregex_iterator f(flags.begin(), flags.end(), quoted); f != stop; ++f)
            set.insert("--" + (*f)[1].str());
        set.insert("--help");  // every subcommand answers --help
    }
    REQUIRE(out.size() > 10);
    return out;
}

Parsed parse_bash(const std::string& text) {
    Parsed p;
    static const std::regex list_re(R"re(local subcommands="([^"]*)")re");
    std::smatch m;
    if (std::regex_search(text, m, list_re))
        for (const auto& c : split_ws(m[1].str())) p.listed.insert(c);

    bool in_case = false;
    std::vector<std::string> current;
    for (const auto& t : split_lines(text)) {
        if (t.find("case \"${subcmd}\" in") != std::string::npos) { in_case = true; continue; }
        if (!in_case) continue;
        if (t == "esac") break;
        if (t == ";;") { current.clear(); continue; }
        if (is_case_arm(t)) {
            current = split_on(t.substr(0, t.size() - 1), '|');
            for (const auto& c : current) p.flags[c];
            continue;
        }
        for (const auto& c : current)
            for (const auto& f : find_flags(t)) p.flags[c].insert(f);
    }
    return p;
}

Parsed parse_zsh(const std::string& text) {
    Parsed p;
    bool in_list = false, in_case = false;
    std::vector<std::string> current;
    static const std::regex entry(R"re(^'([a-z-]+):)re");
    for (const auto& t : split_lines(text)) {
        if (t.find("subcommands=(") != std::string::npos) { in_list = true; continue; }
        if (in_list) {
            if (t == ")") { in_list = false; continue; }
            std::smatch m;
            if (std::regex_search(t, m, entry)) p.listed.insert(m[1].str());
            continue;
        }
        if (t.find("case $words[1] in") != std::string::npos) { in_case = true; continue; }
        if (!in_case) continue;
        if (t == "esac") break;
        if (t == ";;") { current.clear(); continue; }
        if (is_case_arm(t)) {
            current = split_on(t.substr(0, t.size() - 1), '|');
            for (const auto& c : current) p.flags[c];
            continue;
        }
        for (const auto& c : current)
            for (const auto& f : find_flags(t)) p.flags[c].insert(f);
    }
    return p;
}

Parsed parse_fish(const std::string& text) {
    // Join backslash-continued lines so each `complete` is one logical line.
    std::string joined;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == '\n') { joined += ' '; ++i; }
        else joined += text[i];
    }

    Parsed p;
    std::vector<std::string> loop_subs;
    bool in_loop = false;
    static const std::regex top_re(R"re(-n __projot_no_subcommand -a ([a-z-]+))re");
    static const std::regex from_re(R"re(__fish_seen_subcommand_from (\$sub|[a-z-]+))re");
    static const std::regex long_re(R"re( -l ([a-z-]+))re");
    for (const auto& t : split_lines(joined)) {
        std::smatch m;
        if (t.rfind("not __fish_seen_subcommand_from ", 0) == 0) {
            auto names = split_ws(t.substr(std::string("not __fish_seen_subcommand_from ").size()));
            p.listed.insert(names.begin(), names.end());
            continue;
        }
        if (t.rfind("for sub in ", 0) == 0) {
            loop_subs = split_ws(t.substr(std::string("for sub in ").size()));
            in_loop = true;
            continue;
        }
        if (t == "end") { in_loop = false; loop_subs.clear(); continue; }
        if (t.rfind("complete", 0) != 0) continue;

        if (std::regex_search(t, m, top_re)) continue;  // top-level command list entry
        std::smatch from;
        if (!std::regex_search(t, from, from_re)) continue;
        std::smatch flag;
        if (!std::regex_search(t, flag, long_re)) continue;
        std::vector<std::string> cmds;
        if (from[1].str() == "$sub") { if (in_loop) cmds = loop_subs; }
        else cmds.push_back(from[1].str());
        for (const auto& c : cmds) p.flags[c].insert("--" + flag[1].str());
    }

    // Top-level entries are read separately so a command with no flags still counts.
    for (std::sregex_iterator it(joined.begin(), joined.end(), top_re), stop; it != stop; ++it)
        p.listed.insert((*it)[1].str());
    return p;
}

Parsed parse_ps1(const std::string& text) {
    Parsed p;
    bool in_list = false, in_flags = false;
    static const std::regex word(R"re('([a-z-]+)')re");
    static const std::regex row(R"re(^'([a-z-]+)'\s*=\s*@\((.*)\)$)re");
    for (const auto& t : split_lines(text)) {
        if (t.rfind("$projotSubcommands = @(", 0) == 0) { in_list = true; continue; }
        if (t.rfind("$projotFlags = @{", 0) == 0) { in_flags = true; continue; }
        if (in_list) {
            if (t == ")") { in_list = false; continue; }
            for (std::sregex_iterator it(t.begin(), t.end(), word), stop; it != stop; ++it)
                p.listed.insert((*it)[1].str());
        } else if (in_flags) {
            if (t == "}") { in_flags = false; continue; }
            std::smatch m;
            if (std::regex_match(t, m, row)) {
                auto& set = p.flags[m[1].str()];
                for (const auto& f : find_flags(m[2].str())) set.insert(f);
            }
        }
    }
    return p;
}

std::string diff_sets(const std::set<std::string>& expected, const std::set<std::string>& actual) {
    std::string out;
    for (const auto& e : expected)
        if (!actual.count(e)) out += " missing:" + e;
    for (const auto& a : actual)
        if (!expected.count(a)) out += " unexpected:" + a;
    return out;
}

void check_script(const std::string& label, const Parsed& parsed) {
    const FlagMap truth = source_flags();

    std::set<std::string> commands;
    for (const auto& [cmd, _] : truth) commands.insert(cmd);

    CHECK_MESSAGE(diff_sets(commands, parsed.listed).empty(),
                  label << " command list:" << diff_sets(commands, parsed.listed));

    // Plain variables, not structured bindings: the CHECK macros capture them in a
    // lambda, which is C++20 for bindings and fails under clang -Werror.
    for (const auto& entry : truth) {
        const std::string& cmd = entry.first;
        const std::set<std::string>& expected = entry.second;
        auto it = parsed.flags.find(cmd);
        std::set<std::string> actual = it == parsed.flags.end() ? std::set<std::string>{} : it->second;
        CHECK_MESSAGE(diff_sets(expected, actual).empty(),
                      label << " flags for '" << cmd << "':" << diff_sets(expected, actual));
    }
    for (const auto& entry : parsed.flags) {
        const std::string& cmd = entry.first;
        CHECK_MESSAGE(truth.count(cmd), label << " has flags for unknown command '" << cmd << "'");
    }
}

}  // namespace

TEST_CASE("completions_source_table_is_readable") {
    FlagMap truth = source_flags();
    CHECK(truth.count("set-teams-post"));
    CHECK(truth.at("summarize").count("--today"));
}

TEST_CASE("completions_bash_matches_cli") {
    check_script("projot.bash", parse_bash(read_text("completions/projot.bash")));
}

TEST_CASE("completions_zsh_matches_cli") {
    check_script("_projot", parse_zsh(read_text("completions/_projot")));
}

TEST_CASE("completions_fish_matches_cli") {
    check_script("projot.fish", parse_fish(read_text("completions/projot.fish")));
}

TEST_CASE("completions_powershell_matches_cli") {
    check_script("projot.ps1", parse_ps1(read_text("completions/projot.ps1")));
}

TEST_CASE("completions_offer_on_off_for_teams_post") {
    for (const char* file : {"completions/projot.bash", "completions/_projot",
                             "completions/projot.fish", "completions/projot.ps1"}) {
        std::string text = read_text(file);
        INFO(file);
        // PowerShell spells its value list as @('on', 'off'); the others use "on off".
        CHECK((text.find("on off") != std::string::npos ||
               text.find("'on', 'off'") != std::string::npos));
    }
}
