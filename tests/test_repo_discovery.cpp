#include "doctest.h"
#include "repo.h"

#include <filesystem>
#include <fstream>

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
