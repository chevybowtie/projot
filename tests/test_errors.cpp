#include "doctest.h"
#include "commands.h"
#include "cli.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

struct ErrorTempRepo {
    fs::path path;
    fs::path original;

    explicit ErrorTempRepo(const std::string& name, bool with_git = true) {
        path = fs::temp_directory_path() / ("projot_err_" + name);
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
        if (with_git) fs::create_directories(path / ".git", ec);
        original = fs::current_path();
        fs::current_path(path);
    }
    ~ErrorTempRepo() {
        std::error_code ec;
        fs::current_path(original, ec);
        fs::remove_all(path, ec);
    }
};

static Args make_err_args(const std::string& sub,
    std::initializer_list<std::pair<std::string,std::string>> flags = {}) {
    Args a; a.subcommand = sub;
    for (const auto& [k, v] : flags) a.flags[k].push_back(v);
    return a;
}

// ── not in a git repo ─────────────────────────────────────────────────────────

TEST_CASE("not_in_git_repo") {
    ErrorTempRepo repo("not_in_git_repo", /*with_git=*/false);
    // Any project command should fail
    CHECK(cmd_add_todo(make_err_args("add-todo", {{"text", "T"}})) != 0);
}

// ── missing config ────────────────────────────────────────────────────────────

TEST_CASE("missing_config") {
    ErrorTempRepo repo("missing_config");
    // .git exists but no .projot/config
    CHECK(cmd_add_todo(make_err_args("add-todo", {{"text", "T"}})) != 0);
}

// ── missing notes file ────────────────────────────────────────────────────────

TEST_CASE("missing_notes_file") {
    ErrorTempRepo repo("missing_notes_file");
    fs::create_directories(repo.path / ".projot");
    // Config has ranp but notes file doesn't exist
    std::ofstream cfg(repo.path / ".projot" / "config");
    cfg << "config_version = 1\napp_id = App\nranp = 12345\nname = P\nitrack = 1\n";
    cfg.close();
    // .projot/12345.md does NOT exist
    CHECK(cmd_add_todo(make_err_args("add-todo", {{"text", "T"}})) != 0);
}

// ── invalid todo ID ───────────────────────────────────────────────────────────

TEST_CASE("invalid_todo_id") {
    ErrorTempRepo repo("invalid_todo_id");
    fs::create_directories(repo.path / ".projot");
    std::ofstream cfg(repo.path / ".projot" / "config");
    cfg << "config_version = 1\napp_id = App\nranp = 1\nname = P\nitrack = 1\n";
    cfg.close();
    std::ofstream notes(repo.path / ".projot" / "1.md");
    notes << "# Project: P\n- RANP: 1\n- iTrack: 1\n- App ID: App\n"
             "- Created: 2025-01-01\n\n## Links\n\n## Todos\n\n"
             "1. [ ] Only todo\n   - Created: 2025-01-01\n   - Notes:\n\n";
    notes.close();
    // ID 99 doesn't exist
    CHECK(cmd_complete(make_err_args("complete", {{"todo", "99"}})) != 0);
}

// ── help exits zero ───────────────────────────────────────────────────────────

TEST_CASE("help_exits_zero") {
    Args a; a.help_requested = true;
    CHECK(cmd_init(a) == 0);
    CHECK(cmd_new(a) == 0);
    CHECK(cmd_add_todo(a) == 0);
    CHECK(cmd_list(a) == 0);
    CHECK(cmd_complete(a) == 0);
    CHECK(cmd_add_note(a) == 0);
    CHECK(cmd_set_link(a) == 0);
    CHECK(cmd_set_app_id(a) == 0);
    CHECK(cmd_add_github(a) == 0);
    CHECK(cmd_add_swagger(a) == 0);
    CHECK(cmd_add_blizzard(a) == 0);
    CHECK(cmd_install_hook(a) == 0);
}

// ── no subcommand → non-zero ──────────────────────────────────────────────────

TEST_CASE("no_args_exits_nonzero") {
    char arg0[] = "projot";
    char* argv[] = {arg0, nullptr};
    Args a = parse_args(1, argv);
    CHECK(a.subcommand.empty());
    CHECK_FALSE(a.help_requested);
}

// ── unknown flag ──────────────────────────────────────────────────────────────

TEST_CASE("unknown_flag") {
    // In main.cpp the valid_flags map catches unknown flags. Here we verify that
    // the Args struct records single-dash tokens as unknown_flags.
    char arg0[] = "projot";
    char arg1[] = "add-todo";
    char arg2[] = "-bogus";
    char* argv[] = {arg0, arg1, arg2, nullptr};
    Args a = parse_args(3, argv);
    CHECK(!a.unknown_flags.empty());
    CHECK(a.unknown_flags[0] == "-bogus");
}

// ── version exits zero ────────────────────────────────────────────────────────

TEST_CASE("version_exits_zero") {
    char arg0[] = "projot";
    char arg1[] = "--version";
    char* argv[] = {arg0, arg1, nullptr};
    Args a = parse_args(2, argv);
    CHECK(a.version_requested);
}

// ── subcommand help exits zero ────────────────────────────────────────────────

TEST_CASE("subcommand_help_exits_zero") {
    char arg0[] = "projot";
    char arg1[] = "add-todo";
    char arg2[] = "--help";
    char* argv[] = {arg0, arg1, arg2, nullptr};
    Args a = parse_args(3, argv);
    CHECK(a.subcommand == "add-todo");
    CHECK(a.help_requested);
    CHECK(cmd_add_todo(a) == 0);
}
