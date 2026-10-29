// Tests when `render` and `summarize` post to Teams.
//
// The posting itself happens in mcp/teams-sync.js (covered by mcp/test.js). What is
// checked here is the decision in the C++ side: whether the script is launched at all,
// and with which arguments. A fake `node` earlier on PATH records its arguments, so
// nothing touches the network and the real script is never run.
//
// POSIX only: the fake is a shell script.

#ifndef _WIN32

#include "doctest.h"
#include "commands.h"
#include "utils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const char* const kSyncUrl = "https://flow.example.com/invoke?sig=SECRET";

// Prepends a directory holding a recording fake `node` to PATH for its lifetime.
struct FakeNode {
    fs::path dir;
    fs::path log;
    std::string saved_path;

    explicit FakeNode(const std::string& name) {
        dir = fs::temp_directory_path() / ("projot_fakenode_" + name);
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        log = dir / "calls.log";

        {
            std::ofstream script(dir / "node");
            script << "#!/bin/sh\n"
                   << "echo CALL >> '" << log.string() << "'\n"
                   << "for a in \"$@\"; do echo \"$a\" >> '" << log.string() << "'; done\n";
        }
        fs::permissions(dir / "node",
                        fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                        fs::perms::others_read | fs::perms::others_exec);

        const char* old = std::getenv("PATH");
        saved_path = old ? old : "";
        setenv("PATH", (dir.string() + ":" + saved_path).c_str(), 1);
    }
    ~FakeNode() {
        setenv("PATH", saved_path.c_str(), 1);
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    // Each launch writes "CALL" followed by node's arguments, one per line.
    std::vector<std::vector<std::string>> calls() const {
        std::vector<std::vector<std::string>> out;
        std::ifstream f(log);
        std::string line;
        while (std::getline(f, line)) {
            if (line == "CALL") out.emplace_back();
            else if (!out.empty()) out.back().push_back(line);
        }
        return out;
    }
};

struct PostRepo {
    fs::path path;
    fs::path original;

    explicit PostRepo(const std::string& name) {
        path = fs::temp_directory_path() / ("projot_post_" + name);
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path / ".git" / "hooks", ec);
        original = fs::current_path();
        fs::current_path(path);

        Args init;
        init.subcommand = "init";
        init.flags["app-id"].push_back("TestApp");
        REQUIRE(cmd_init(init) == 0);

        Args np;
        np.subcommand = "new";
        np.flags["rpm"].push_back("12345");
        np.flags["name"].push_back("Test Project");
        np.flags["jira"].push_back("67890");
        np.flags["no-hook"].push_back("true");
        REQUIRE(cmd_new(np) == 0);
    }
    ~PostRepo() {
        std::error_code ec;
        fs::current_path(original, ec);
        fs::remove_all(path, ec);
    }

    void set_sync_url() {
        Args a;
        a.subcommand = "set-teams-webhook";
        a.positional.push_back(kSyncUrl);
        REQUIRE(cmd_set_teams_webhook(a) == 0);
    }

    void set_post(const std::string& flag, const std::string& value) {
        Args a;
        a.subcommand = "set-teams-post";
        a.flags[flag].push_back(value);
        REQUIRE(cmd_set_teams_post(a) == 0);
    }

    int render() {
        Args a;
        a.subcommand = "render";
        return cmd_render(a);
    }

    int summarize() {
        Args a;
        a.subcommand = "summarize";
        return cmd_summarize(a);
    }
};

bool contains(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) if (x == s) return true;
    return false;
}

}  // namespace

// ── render ───────────────────────────────────────────────────────────────────

TEST_CASE("render_posts_to_teams_when_url_set") {
    FakeNode node("render_posts");
    PostRepo repo("render_posts");
    repo.set_sync_url();

    CHECK(repo.render() == 0);

    auto calls = node.calls();
    REQUIRE(calls.size() == 1);
    // node <script> <config> <notes>, and no summary arguments for a render post.
    REQUIRE(calls[0].size() == 3);
    CHECK(calls[0][0].find("teams-sync.js") != std::string::npos);
    CHECK(!contains(calls[0], "--summary"));
}

TEST_CASE("render_never_puts_the_sync_url_on_the_command_line") {
    FakeNode node("render_secret");
    PostRepo repo("render_secret");
    repo.set_sync_url();
    repo.render();

    auto calls = node.calls();
    REQUIRE(calls.size() == 1);
    for (const auto& arg : calls[0])
        CHECK(arg.find("SECRET") == std::string::npos);
}

TEST_CASE("render_does_not_post_without_sync_url") {
    FakeNode node("render_no_url");
    PostRepo repo("render_no_url");

    CHECK(repo.render() == 0);
    CHECK(node.calls().empty());
}

TEST_CASE("render_does_not_post_when_render_toggle_off") {
    FakeNode node("render_off");
    PostRepo repo("render_off");
    repo.set_sync_url();
    repo.set_post("render", "off");

    CHECK(repo.render() == 0);
    CHECK(node.calls().empty());
}

TEST_CASE("render_still_posts_when_only_summarize_is_off") {
    FakeNode node("render_summarize_off");
    PostRepo repo("render_summarize_off");
    repo.set_sync_url();
    repo.set_post("summarize", "off");

    CHECK(repo.render() == 0);
    CHECK(node.calls().size() == 1);
}

TEST_CASE("render_posts_again_after_toggle_turned_back_on") {
    FakeNode node("render_back_on");
    PostRepo repo("render_back_on");
    repo.set_sync_url();
    repo.set_post("render", "off");
    repo.render();
    REQUIRE(node.calls().empty());

    repo.set_post("render", "on");
    repo.render();
    CHECK(node.calls().size() == 1);
}

// ── summarize ────────────────────────────────────────────────────────────────

TEST_CASE("summarize_posts_summary_with_todays_date") {
    FakeNode node("summarize_posts");
    PostRepo repo("summarize_posts");
    repo.set_sync_url();

    CHECK(repo.summarize() == 0);

    auto calls = node.calls();
    REQUIRE(calls.size() == 1);
    CHECK(contains(calls[0], "--summary"));
    // The date is passed in so the post matches what was printed, whatever the
    // script's own clock says.
    CHECK(contains(calls[0], "--today=" + date_today()));
}

TEST_CASE("summarize_never_puts_the_sync_url_on_the_command_line") {
    FakeNode node("summarize_secret");
    PostRepo repo("summarize_secret");
    repo.set_sync_url();
    repo.summarize();

    auto calls = node.calls();
    REQUIRE(calls.size() == 1);
    for (const auto& arg : calls[0])
        CHECK(arg.find("SECRET") == std::string::npos);
}

TEST_CASE("summarize_does_not_post_without_sync_url") {
    FakeNode node("summarize_no_url");
    PostRepo repo("summarize_no_url");

    CHECK(repo.summarize() == 0);
    CHECK(node.calls().empty());
}

TEST_CASE("summarize_does_not_post_when_summarize_toggle_off") {
    FakeNode node("summarize_off");
    PostRepo repo("summarize_off");
    repo.set_sync_url();
    repo.set_post("summarize", "off");

    CHECK(repo.summarize() == 0);
    CHECK(node.calls().empty());
}

TEST_CASE("summarize_still_posts_when_only_render_is_off") {
    FakeNode node("summarize_render_off");
    PostRepo repo("summarize_render_off");
    repo.set_sync_url();
    repo.set_post("render", "off");

    CHECK(repo.summarize() == 0);
    CHECK(node.calls().size() == 1);
}

#endif  // _WIN32
