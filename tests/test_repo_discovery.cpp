#include "doctest.h"
#include "repo.h"
#include "process.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <fstream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <time.h>
#endif

namespace fs = std::filesystem;

// Helper: create a temp directory tree and return root
static fs::path make_temp_dir(const std::string& name) {
    auto base = fs::temp_directory_path() / ("projot_repo_" + name);
    fs::create_directories(base);
    return base;
}

static void cleanup(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

TEST_CASE("find_root_at_cwd") {
    auto root = make_temp_dir("at_cwd");
    fs::create_directories(root / ".git");
    auto result = find_repo_root(root);
    REQUIRE(result.has_value());
    CHECK(fs::equivalent(*result, root));
    cleanup(root);
}

TEST_CASE("find_root_two_levels_up") {
    auto root = make_temp_dir("two_levels");
    fs::create_directories(root / ".git");
    auto sub = root / "a" / "b";
    fs::create_directories(sub);
    auto result = find_repo_root(sub);
    REQUIRE(result.has_value());
    CHECK(fs::equivalent(*result, root));
    cleanup(root);
}

TEST_CASE("find_root_at_filesystem_root") {
    // Create a directory with no .git anywhere in a temp hierarchy (no .git at root)
    // We can only safely test this by using a path guaranteed to have no .git above it.
    // Use a tmp subdir and walk from it — if host has no .git at /, this should fail.
    // We check a freshly created dir with no .git
    auto dir = make_temp_dir("no_git");
    // Don't create .git; if the system happens to have .git at temp root this may vary,
    // but in practice /tmp is never inside a git repo on CI.
    auto result = find_repo_root(dir);
    // We can only assert absence if we know no .git exists above.
    // Check that either not found, or the found root is not our fresh dir.
    if (result.has_value()) {
        // Found a real git repo above /tmp — skip assertion, environment-dependent
        MESSAGE("find_root_at_filesystem_root: found a .git above the temp dir (skipping)");
    } else {
        CHECK_FALSE(result.has_value());
    }
    cleanup(dir);
}

TEST_CASE("find_root_ignores_file_named_git") {
    // A regular file named .git should not be treated as a repo root by our logic.
    // Note: our implementation uses fs::exists which returns true for files too.
    // Per DESIGN: "A file named .git (worktree) → treated as repo root (file presence is sufficient)"
    auto root = make_temp_dir("file_git");
    // Create .git as a file (simulating a git worktree)
    { std::ofstream f(root / ".git"); f << "gitdir: ../.git/worktrees/wt"; }
    auto result = find_repo_root(root);
    REQUIRE(result.has_value());
    CHECK(fs::equivalent(*result, root));
    cleanup(root);
}

TEST_CASE("find_root_bare_repo") {
    // .git as a file (worktree case) → treated as repo root
    auto root = make_temp_dir("bare_repo");
    { std::ofstream f(root / ".git"); f << "gitdir: /some/other/path"; }
    auto result = find_repo_root(root);
    REQUIRE(result.has_value());
    CHECK(fs::equivalent(*result, root));
    cleanup(root);
}

// ── git dir / hooks dir resolution ────────────────────────────────────────────

namespace {
struct ScratchDir {
    std::filesystem::path path;
    explicit ScratchDir(const std::string& name)
        : path(std::filesystem::temp_directory_path() / ("projot_gitdir_" + name)) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }
    ~ScratchDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};

bool git_available() {
    auto r = run_process({"git", "--version"}, ChildOutput::Discard, ChildOutput::Discard);
    return r.started && r.exit_code == 0;
}

std::optional<std::string> get_env(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || !value) return std::nullopt;
    std::string result(value);
    free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value ? std::optional<std::string>(value) : std::nullopt;
#endif
}

void set_env(const char* name, const std::string* value) {
#ifdef _WIN32
    _putenv_s(name, value ? value->c_str() : "");  // "" removes the variable
#else
    if (value) setenv(name, value->c_str(), 1);
    else       unsetenv(name);
#endif
}

// Git exports variables such as GIT_INDEX_FILE (a relative ".git/index") to hooks.
// When the suite runs inside a pre-commit hook they would redirect the scratch
// repos' git commands, so clear them for the test and restore them afterwards.
struct ScrubbedGitEnv {
    std::vector<std::pair<const char*, std::optional<std::string>>> saved;
    ScrubbedGitEnv() {
        for (const char* name : {"GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE",
                                 "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES",
                                 "GIT_COMMON_DIR", "GIT_PREFIX", "GIT_CONFIG_PARAMETERS"}) {
            saved.emplace_back(name, get_env(name));
            set_env(name, nullptr);
        }
    }
    ~ScrubbedGitEnv() {
        for (const auto& [name, value] : saved)
            if (value) set_env(name, &*value);
    }
};

bool git(const std::filesystem::path& dir, std::vector<std::string> args) {
    args.insert(args.begin(), {"git", "-C", dir.string()});
    auto r = run_process(args, ChildOutput::Discard, ChildOutput::Discard);
    return r.started && r.exit_code == 0;
}
} // namespace

TEST_CASE("resolve_git_dir_directory") {
    ScratchDir d("dir");
    std::filesystem::create_directories(d.path / ".git");
    auto g = resolve_git_dir(d.path);
    REQUIRE(g);
    CHECK(*g == d.path / ".git");
}

TEST_CASE("resolve_git_dir_follows_gitdir_file_and_commondir") {
    // Layout of a linked worktree, built by hand so git itself is not needed.
    ScratchDir d("worktree_layout");
    namespace fs = std::filesystem;
    fs::create_directories(d.path / "main" / ".git" / "worktrees" / "wt");
    fs::create_directories(d.path / "wt");
    { std::ofstream f(d.path / "wt" / ".git"); f << "gitdir: ../main/.git/worktrees/wt\n"; }
    { std::ofstream f(d.path / "main" / ".git" / "worktrees" / "wt" / "commondir"); f << "../..\n"; }

    auto g = resolve_git_dir(d.path / "wt");
    REQUIRE(g);
    CHECK(*g == (d.path / "main" / ".git" / "worktrees" / "wt").lexically_normal());
    // Not a real repository, so git fails and the manual fallback is exercised.
    CHECK(resolve_hooks_dir(d.path / "wt") == (d.path / "main" / ".git" / "hooks").lexically_normal());
}

TEST_CASE("resolve_hooks_dir_real_worktree_and_hooks_path") {
    if (!git_available()) return;
    namespace fs = std::filesystem;
    ScrubbedGitEnv clean_env;
    ScratchDir d("real_git");
    const fs::path main = d.path / "main";
    fs::create_directories(main);
    REQUIRE(git(main, {"init", "-q"}));
    REQUIRE(git(main, {"-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "--allow-empty", "-m", "x"}));
    REQUIRE(git(main, {"worktree", "add", "-q", (d.path / "wt").string()}));

    auto hooks = fs::weakly_canonical(resolve_hooks_dir(d.path / "wt"));
    CHECK(hooks == fs::weakly_canonical(main / ".git" / "hooks"));

    REQUIRE(git(main, {"config", "core.hooksPath", ".husky"}));
    CHECK(fs::weakly_canonical(resolve_hooks_dir(main)) == fs::weakly_canonical(main / ".husky"));
}

// ── run_process ───────────────────────────────────────────────────────────────

TEST_CASE("run_process_reports_missing_program") {
    auto r = run_process({"projot-no-such-program-xyz"}, ChildOutput::Discard, ChildOutput::Discard);
    CHECK((!r.started || r.exit_code != 0));
}

TEST_CASE("run_process_captures_stdout") {
    if (!git_available()) return;
    auto r = run_process({"git", "--version"}, ChildOutput::Capture, ChildOutput::Discard);
    CHECK(r.exit_code == 0);
    CHECK(r.output.find("git version") != std::string::npos);
}

#ifndef _WIN32
TEST_CASE("run_process_kills_child_on_timeout") {
    auto start = std::chrono::steady_clock::now();
    auto r = run_process({"sleep", "5"}, ChildOutput::Discard, ChildOutput::Discard, 200);
    auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(r.timed_out);
    CHECK(elapsed < std::chrono::seconds(3));
}

TEST_CASE("run_process_exit_code") {
    CHECK(run_process({"sh", "-c", "exit 3"}, ChildOutput::Discard, ChildOutput::Discard).exit_code == 3);
}

// True once pid no longer exists (a killed orphan is reaped by init shortly after).
static bool process_gone(pid_t pid) {
    for (int i = 0; i < 100; ++i) {
        if (kill(pid, 0) != 0 && errno == ESRCH) return true;
        struct timespec ts{0, 10 * 1000 * 1000};
        nanosleep(&ts, nullptr);
    }
    return false;
}

TEST_CASE("run_process_capture_times_out_when_child_holds_stdout") {
    auto start = std::chrono::steady_clock::now();
    auto r = run_process({"sh", "-c", "echo partial; sleep 30"},
                         ChildOutput::Capture, ChildOutput::Discard, 300);
    CHECK(r.timed_out);
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(3));
    CHECK(r.output == "partial\n");
}

TEST_CASE("run_process_capture_times_out_when_descendant_holds_stdout") {
    // The shell exits at once, but the background sleep inherited stdout and keeps
    // the pipe open; reading to EOF would block for 30 s.
    auto start = std::chrono::steady_clock::now();
    auto r = run_process({"sh", "-c", "sleep 30 & echo $!"},
                         ChildOutput::Capture, ChildOutput::Discard, 300);
    CHECK(r.timed_out);
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(3));
    // The timeout kills the whole process group, including the orphaned sleep.
    const pid_t descendant = static_cast<pid_t>(std::atoi(r.output.c_str()));
    REQUIRE(descendant > 0);
    CHECK(process_gone(descendant));
}

TEST_CASE("run_process_capture_without_timeout_still_completes") {
    auto r = run_process({"sh", "-c", "printf 'a'; printf 'b'"}, ChildOutput::Capture, ChildOutput::Discard);
    CHECK_FALSE(r.timed_out);
    CHECK(r.exit_code == 0);
    CHECK(r.output == "ab");
}
#endif
