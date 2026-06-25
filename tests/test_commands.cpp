#include "doctest.h"
#include "commands.h"
#include "commands_internal.h"
#include "config.h"
#include "markdown.h"
#include "todo.h"
#include "utils.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// ── Test helpers ──────────────────────────────────────────────────────────────

struct TempRepo {
    fs::path path;
    fs::path original;

    explicit TempRepo(const std::string& name) {
        path = fs::temp_directory_path() / ("projot_cmd_" + name);
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path / ".git" / "hooks", ec);
        original = fs::current_path();
        fs::current_path(path);
    }
    ~TempRepo() {
        std::error_code ec;
        fs::current_path(original, ec);
        fs::remove_all(path, ec);
    }

    // Run init with app-id and return exit code.
    int init(const std::string& app_id = "TestApp") {
        Args a;
        a.subcommand = "init";
        a.flags["app-id"].push_back(app_id);
        return cmd_init(a);
    }

    // Run new with required fields and return exit code.
    int new_project(const std::string& rpm = "12345",
                    const std::string& name = "Test Project",
                    const std::string& itrack = "67890",
                    bool no_hook = true) {
        Args a;
        a.subcommand = "new";
        a.flags["rpm"].push_back(rpm);
        a.flags["name"].push_back(name);
        a.flags["itrack"].push_back(itrack);
        if (no_hook) a.flags["no-hook"].push_back("true");
        return cmd_new(a);
    }
};

static Args make_args(const std::string& sub,
                      std::initializer_list<std::pair<std::string,std::string>> flags = {},
                      const std::string& positional = "") {
    Args a;
    a.subcommand = sub;
    for (const auto& [k, v] : flags) a.flags[k].push_back(v);
    if (!positional.empty()) a.positional.push_back(positional);
    return a;
}

// ── init ──────────────────────────────────────────────────────────────────────

TEST_CASE("init_creates_projot_dir") {
    TempRepo repo("init_creates_projot_dir");
    CHECK(repo.init() == 0);
    CHECK(fs::exists(repo.path / ".projot"));
}

TEST_CASE("init_writes_config") {
    TempRepo repo("init_writes_config");
    repo.init("MyApp");
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.app_id == "MyApp");
    CHECK(cfg.config_version == PROJOT_CONFIG_SCHEMA_VERSION);
}

TEST_CASE("init_with_github_url") {
    TempRepo repo("init_with_github_url");
    Args a = make_args("init", {{"app-id", "App"}, {"github", "https://github.com/org/repo"}});
    cmd_init(a);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    REQUIRE(cfg.github.size() == 1);
    CHECK(cfg.github[0] == "https://github.com/org/repo");
}

TEST_CASE("init_with_multiple_github") {
    TempRepo repo("init_with_multiple_github");
    Args a;
    a.subcommand = "init";
    a.flags["app-id"].push_back("App");
    a.flags["github"].push_back("https://github.com/org/repo-one");
    a.flags["github"].push_back("https://github.com/org/repo-two");
    cmd_init(a);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.github.size() == 2);
}

TEST_CASE("init_fails_if_already_init") {
    TempRepo repo("init_fails_if_already_init");
    repo.init();
    int ret = repo.init();
    CHECK(ret != 0);
}

TEST_CASE("init_requires_app_id") {
    TempRepo repo("init_requires_app_id");
    Args a = make_args("init");  // no app-id
    CHECK(cmd_init(a) != 0);
}

// ── new ───────────────────────────────────────────────────────────────────────

TEST_CASE("new_writes_project_fields") {
    TempRepo repo("new_writes_project_fields");
    repo.init();
    repo.new_project("11111", "My Proj", "22222");
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.rpm == "11111");
    CHECK(cfg.name == "My Proj");
    CHECK(cfg.itrack == "22222");
}

TEST_CASE("new_creates_notes_file") {
    TempRepo repo("new_creates_notes_file");
    repo.init();
    repo.new_project("33333");
    CHECK(fs::exists(repo.path / ".projot" / "33333.md"));
}

TEST_CASE("new_notes_file_has_correct_header") {
    TempRepo repo("new_notes_file_has_correct_header");
    repo.init();
    repo.new_project("44444", "Widget Redesign", "55555");
    Project proj;
    parse_markdown((repo.path / ".projot" / "44444.md").string(), proj);
    CHECK(proj.name == "Widget Redesign");
    CHECK(proj.rpm == "44444");
}

TEST_CASE("new_with_teams_url") {
    TempRepo repo("new_with_teams_url");
    repo.init();
    Args a;
    a.subcommand = "new";
    a.flags["rpm"].push_back("66666");
    a.flags["name"].push_back("T");
    a.flags["itrack"].push_back("1");
    a.flags["teams"].push_back("https://teams.microsoft.com/t");
    a.flags["no-hook"].push_back("true");
    cmd_new(a);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.link_urls["teams"] == "https://teams.microsoft.com/t");
}

TEST_CASE("new_with_teams_sync_url") {
    TempRepo repo("new_with_teams_sync_url");
    repo.init();
    Args a;
    a.subcommand = "new";
    a.flags["rpm"].push_back("66667");
    a.flags["name"].push_back("T");
    a.flags["itrack"].push_back("2");
    a.flags["teams-sync-url"].push_back("https://flow.example.com/invoke");
    a.flags["no-hook"].push_back("true");
    CHECK(cmd_new(a) == 0);

    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.teams_sync_url == "https://flow.example.com/invoke");
}

TEST_CASE("new_fails_if_rpm_set") {
    TempRepo repo("new_fails_if_rpm_set");
    repo.init();
    repo.new_project("77777");
    int ret = repo.new_project("88888");
    CHECK(ret != 0);
}

TEST_CASE("new_fails_without_required_flags") {
    TempRepo repo("new_fails_without_required_flags");
    repo.init();
    // Missing --itrack
    Args a = make_args("new", {{"rpm", "1"}, {"name", "P"}});
    CHECK(cmd_new(a) != 0);
}

TEST_CASE("new_inherits_repo_fields") {
    TempRepo repo("new_inherits_repo_fields");
    Args init_a;
    init_a.subcommand = "init";
    init_a.flags["app-id"].push_back("RepoApp");
    init_a.flags["github"].push_back("https://github.com/org/r");
    cmd_init(init_a);
    repo.new_project("99999");
    Project proj;
    parse_markdown((repo.path / ".projot" / "99999.md").string(), proj);
    CHECK(!proj.github_urls.empty());
    CHECK(proj.github_urls[0] == "https://github.com/org/r");
}

// ── close ─────────────────────────────────────────────────────────────────────

TEST_CASE("close_clears_project_fields") {
    TempRepo repo("close_clears_project_fields");
    repo.init("MyApp");
    repo.new_project("12345", "Test Proj", "67890");
    int ret = cmd_close(make_args("close"));
    CHECK(ret == 0);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    // Project fields cleared
    CHECK(cfg.rpm == "");
    CHECK(cfg.name == "");
    CHECK(cfg.itrack == "");
    CHECK(cfg.created == "");
    CHECK(cfg.links.empty());
    // Repo-level fields preserved
    CHECK(cfg.app_id == "MyApp");
}

TEST_CASE("close_archives_notes_file") {
    TempRepo repo("close_archives_notes_file");
    repo.init();
    repo.new_project("54321", "Archive Test", "11111");
    // Verify original exists
    CHECK(fs::exists(repo.path / ".projot" / "54321.md"));
    int ret = cmd_close(make_args("close"));
    CHECK(ret == 0);
    // Verify archived exists, original gone
    CHECK(fs::exists(repo.path / ".projot" / "archive" / "54321.md"));
    CHECK(!fs::exists(repo.path / ".projot" / "54321.md"));
}

TEST_CASE("close_clears_azure_resources") {
    TempRepo repo("close_clears_azure_resources");
    repo.init();
    repo.new_project("azure1", "Azure Test", "22222");
    // Add an Azure resource
    Args az_a;
    az_a.subcommand = "add-azure";
    az_a.flags["type"].push_back("subscription");
    az_a.flags["url"].push_back("https://portal.azure.com/sub");
    cmd_add_azure(az_a);
    // Verify it's there
    Config cfg_before;
    parse_config((repo.path / ".projot" / "config").string(), cfg_before);
    REQUIRE(!cfg_before.azure_subscription.empty());
    // Close and verify cleared
    cmd_close(make_args("close"));
    Config cfg_after;
    parse_config((repo.path / ".projot" / "config").string(), cfg_after);
    CHECK(cfg_after.azure_subscription.empty());
}

TEST_CASE("close_requires_active_project") {
    TempRepo repo("close_requires_active_project");
    repo.init();
    // No project created
    int ret = cmd_close(make_args("close"));
    CHECK(ret != 0);
}

TEST_CASE("new_blocked_when_project_active") {
    TempRepo repo("new_blocked_when_project_active");
    repo.init();
    repo.new_project("proj1", "First Project", "11111");
    // Try to create another project
    int ret = repo.new_project("proj2", "Second Project", "22222");
    CHECK(ret != 0);
    // Verify first project is still active
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.rpm == "proj1");
    CHECK(cfg.name == "First Project");
}

// ── add-todo ──────────────────────────────────────────────────────────────────

TEST_CASE("add_todo_appends") {
    TempRepo repo("add_todo_appends");
    repo.init(); repo.new_project("1");
    Args a = make_args("add-todo", {}, "My first todo");
    CHECK(cmd_add_todo(a) == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "1.md").string(), proj);
    REQUIRE(proj.todos.size() == 1);
    CHECK(proj.todos[0].text == "My first todo");
    CHECK(!proj.todos[0].created_date.empty());
}

TEST_CASE("add_todo_stable_id") {
    TempRepo repo("add_todo_stable_id");
    repo.init(); repo.new_project("2");
    cmd_add_todo(make_args("add-todo", {}, "First"));
    cmd_add_todo(make_args("add-todo", {}, "Second"));
    Project proj;
    parse_markdown((repo.path / ".projot" / "2.md").string(), proj);
    REQUIRE(proj.todos.size() == 2);
    CHECK(proj.todos[0].id == 1);
    CHECK(proj.todos[1].id == 2);
}

// ── list ─────────────────────────────────────────────────────────────────────

TEST_CASE("list_default_shows_open") {
    TempRepo repo("list_default_shows_open");
    repo.init(); repo.new_project("3");
    cmd_add_todo(make_args("add-todo", {}, "Open one"));
    cmd_add_todo(make_args("add-todo", {}, "Open two"));
    cmd_complete(make_args("complete", {{"todo", "1"}}));
    // Just check it returns 0 (output goes to stdout which we can't easily capture here)
    CHECK(cmd_list(make_args("list")) == 0);
}

TEST_CASE("list_closed_flag") {
    TempRepo repo("list_closed_flag");
    repo.init(); repo.new_project("4");
    cmd_add_todo(make_args("add-todo", {}, "T"));
    cmd_complete(make_args("complete", {{"todo", "1"}}));
    CHECK(cmd_list(make_args("list", {{"closed", "true"}})) == 0);
}

TEST_CASE("list_all_flag") {
    TempRepo repo("list_all_flag");
    repo.init(); repo.new_project("5");
    cmd_add_todo(make_args("add-todo", {}, "T"));
    CHECK(cmd_list(make_args("list", {{"all", "true"}})) == 0);
}

// ── complete ─────────────────────────────────────────────────────────────────

TEST_CASE("complete_marks_done") {
    TempRepo repo("complete_marks_done");
    repo.init(); repo.new_project("6");
    cmd_add_todo(make_args("add-todo", {}, "Todo"));
    CHECK(cmd_complete(make_args("complete", {{"todo", "1"}})) == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "6.md").string(), proj);
    REQUIRE(!proj.todos.empty());
    CHECK(proj.todos[0].status == TodoStatus::Done);
    CHECK(!proj.todos[0].completed_date.empty());
}

TEST_CASE("complete_warns_if_already_done") {
    TempRepo repo("complete_warns_if_already_done");
    repo.init(); repo.new_project("7");
    cmd_add_todo(make_args("add-todo", {}, "T"));
    cmd_complete(make_args("complete", {{"todo", "1"}}));
    // Second complete should warn but return 0
    CHECK(cmd_complete(make_args("complete", {{"todo", "1"}})) == 0);
}

// ── add-note ─────────────────────────────────────────────────────────────────

TEST_CASE("add_note_appends") {
    TempRepo repo("add_note_appends");
    repo.init(); repo.new_project("8");
    // Ensure the project config has a date_format so notes are prefixed
    {
        std::ofstream f((repo.path / ".projot" / "config").string(), std::ios::app);
        f << "date_format = YYYY-MM-DD\n";
    }
    cmd_add_todo(make_args("add-todo", {}, "T"));
    CHECK(cmd_add_note(make_args("add-note", {{"todo", "1"}}, "My note")) == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "8.md").string(), proj);
    REQUIRE(!proj.todos.empty());
    REQUIRE(proj.todos[0].notes.size() == 1);
    // Expect note to be prefixed with today's date in configured format
    CHECK(proj.todos[0].notes[0] == date_today() + " - My note");
}

TEST_CASE("add_note_warns_if_closed") {
    TempRepo repo("add_note_warns_if_closed");
    repo.init(); repo.new_project("9");
    // Ensure the project config has a date_format so notes are prefixed
    {
        std::ofstream f((repo.path / ".projot" / "config").string(), std::ios::app);
        f << "date_format = YYYY-MM-DD\n";
    }
    cmd_add_todo(make_args("add-todo", {}, "T"));
    cmd_complete(make_args("complete", {{"todo", "1"}}));
    // Should warn but still return 0 and write note
    CHECK(cmd_add_note(make_args("add-note", {{"todo", "1"}}, "Late")) == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "9.md").string(), proj);
    CHECK(!proj.todos[0].notes.empty());
    CHECK(proj.todos[0].notes[0] == date_today() + " - Late");
}

// ── set-link ─────────────────────────────────────────────────────────────────

TEST_CASE("set_link_new_key") {
    TempRepo repo("set_link_new_key");
    repo.init(); repo.new_project("10");
    CHECK(cmd_set_link(make_args("set-link", {{"key", "teams"}, {"url", "https://teams.com/t"}})) == 0);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.link_urls["teams"] == "https://teams.com/t");
}

TEST_CASE("set_link_update_key") {
    TempRepo repo("set_link_update_key");
    repo.init(); repo.new_project("11");
    cmd_set_link(make_args("set-link", {{"key", "teams"}, {"url", "https://old.com"}}));
    cmd_set_link(make_args("set-link", {{"key", "teams"}, {"url", "https://new.com"}}));
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.link_urls["teams"] == "https://new.com");
    // Should not be duplicated in links list
    int count = 0;
    for (const auto& k : cfg.links) if (k == "teams") ++count;
    CHECK(count == 1);
}

TEST_CASE("set_teams_webhook_writes_sync_url_key") {
    TempRepo repo("set_teams_webhook_writes_sync_url_key");
    repo.init(); repo.new_project("11a");
    CHECK(cmd_set_teams_webhook(make_args("set-teams-webhook", {}, "https://flow.example.com/invoke")) == 0);

    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.teams_sync_url == "https://flow.example.com/invoke");

    std::ifstream f((repo.path / ".projot" / "config").string());
    std::stringstream buffer;
    buffer << f.rdbuf();
    CHECK(buffer.str().find("teams_sync_url = https://flow.example.com/invoke") != std::string::npos);
}

// ── set-app-id ───────────────────────────────────────────────────────────────

TEST_CASE("set_app_id_sets_value") {
    TempRepo repo("set_app_id_sets_value");
    // init with empty app_id so --force not needed
    Args init_a = make_args("init", {{"app-id", "OldApp"}});
    cmd_init(init_a);
    repo.new_project("12");
    CHECK(cmd_set_app_id(make_args("set-app-id", {{"app-id", "NewApp"}, {"force", "true"}})) == 0);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.app_id == "NewApp");
}

TEST_CASE("set_app_id_no_force_fails") {
    TempRepo repo("set_app_id_no_force_fails");
    repo.init("ExistingApp");
    repo.new_project("13");
    // app_id is already set, no --force → should fail
    CHECK(cmd_set_app_id(make_args("set-app-id", {{"app-id", "Other"}})) != 0);
}

TEST_CASE("set_app_id_force_overwrites") {
    TempRepo repo("set_app_id_force_overwrites");
    repo.init("OldApp");
    repo.new_project("14");
    CHECK(cmd_set_app_id(make_args("set-app-id", {{"app-id", "NewApp"}, {"force", "true"}})) == 0);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.app_id == "NewApp");
}

// ── add-github/swagger/blizzard ───────────────────────────────────────────────

TEST_CASE("add_github_deduplicates") {
    TempRepo repo("add_github_deduplicates");
    repo.init(); repo.new_project("15");
    cmd_add_github(make_args("add-github", {{"url", "https://github.com/org/r"}}));
    cmd_add_github(make_args("add-github", {{"url", "https://github.com/org/r"}}));
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.github.size() == 1);
}

TEST_CASE("add_swagger_deduplicates") {
    TempRepo repo("add_swagger_deduplicates");
    repo.init(); repo.new_project("16");
    cmd_add_swagger(make_args("add-swagger", {{"url", "https://api.com/s"}}));
    cmd_add_swagger(make_args("add-swagger", {{"url", "https://api.com/s"}}));
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.swagger.size() == 1);
}

TEST_CASE("add_blizzard_deduplicates") {
    TempRepo repo("add_blizzard_deduplicates");
    repo.init(); repo.new_project("17");
    cmd_add_blizzard(make_args("add-blizzard", {{"url", "https://bliz.com/b"}}));
    cmd_add_blizzard(make_args("add-blizzard", {{"url", "https://bliz.com/b"}}));
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.blizzard.size() == 1);
}

// ── add-azure ─────────────────────────────────────────────────────────────────

TEST_CASE("add_azure_subscription_named") {
    TempRepo repo("add_azure_subscription_named");
    repo.init(); repo.new_project("az1");
    Args a;
    a.subcommand = "add-azure";
    a.flags["type"].push_back("subscription");
    a.flags["name"].push_back("MySub");
    a.flags["url"].push_back("https://portal.azure.com/sub");
    CHECK(cmd_add_azure(a) == 0);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    REQUIRE(cfg.azure_subscription.size() == 1);
    CHECK(cfg.azure_subscription[0] == "MySub|https://portal.azure.com/sub");
}

TEST_CASE("add_azure_private_dns_url_only") {
    TempRepo repo("add_azure_private_dns_url_only");
    repo.init(); repo.new_project("az2");
    Args a;
    a.subcommand = "add-azure";
    a.flags["type"].push_back("private-dns");
    a.flags["url"].push_back("https://portal.azure.com/dns");
    CHECK(cmd_add_azure(a) == 0);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    REQUIRE(cfg.azure_private_dns.size() == 1);
    CHECK(cfg.azure_private_dns[0] == "https://portal.azure.com/dns");
}

TEST_CASE("add_azure_deduplicates") {
    TempRepo repo("add_azure_deduplicates");
    repo.init(); repo.new_project("az3");
    Args a;
    a.subcommand = "add-azure";
    a.flags["type"].push_back("aks");
    a.flags["name"].push_back("my-aks");
    a.flags["url"].push_back("https://portal.azure.com/aks");
    cmd_add_azure(a);
    cmd_add_azure(a);
    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    CHECK(cfg.azure_aks.size() == 1);
}

TEST_CASE("add_azure_invalid_type_fails") {
    TempRepo repo("add_azure_invalid_type_fails");
    repo.init(); repo.new_project("az4");
    Args a;
    a.subcommand = "add-azure";
    a.flags["type"].push_back("cosmos-db");
    a.flags["url"].push_back("https://portal.azure.com/cosmos");
    CHECK(cmd_add_azure(a) != 0);
}

TEST_CASE("add_azure_missing_url_fails") {
    TempRepo repo("add_azure_missing_url_fails");
    repo.init(); repo.new_project("az5");
    Args a;
    a.subcommand = "add-azure";
    a.flags["type"].push_back("storage");
    CHECK(cmd_add_azure(a) != 0);
}

TEST_CASE("add_azure_renders_notes_file") {
    TempRepo repo("add_azure_renders_notes_file");
    repo.init(); repo.new_project("az6");
    Args a;
    a.subcommand = "add-azure";
    a.flags["type"].push_back("key-vault");
    a.flags["name"].push_back("my-kv");
    a.flags["url"].push_back("https://portal.azure.com/kv");
    cmd_add_azure(a);
    // Check that the notes file was re-rendered and includes the Azure section
    std::ifstream f((repo.path / ".projot" / "az6.md").string());
    std::string content((std::istreambuf_iterator<char>(f)), {});
    CHECK(content.find("## Azure") != std::string::npos);
    CHECK(content.find("[my-kv](https://portal.azure.com/kv)") != std::string::npos);
}

TEST_CASE("add_azure_multiple_of_same_type") {
    // Verify that two different subscriptions (PROD + NPRD) are both stored.
    TempRepo repo("add_azure_multiple_of_same_type");
    repo.init(); repo.new_project("az7");

    Args a_prod;
    a_prod.subcommand = "add-azure";
    a_prod.flags["type"].push_back("subscription");
    a_prod.flags["name"].push_back("PROD");
    a_prod.flags["url"].push_back("https://portal.azure.com/subscriptions/prod-id");
    CHECK(cmd_add_azure(a_prod) == 0);

    Args a_nprd;
    a_nprd.subcommand = "add-azure";
    a_nprd.flags["type"].push_back("subscription");
    a_nprd.flags["name"].push_back("NPRD");
    a_nprd.flags["url"].push_back("https://portal.azure.com/subscriptions/nprd-id");
    CHECK(cmd_add_azure(a_nprd) == 0);

    Config cfg;
    parse_config((repo.path / ".projot" / "config").string(), cfg);
    REQUIRE(cfg.azure_subscription.size() == 2);
    CHECK(cfg.azure_subscription[0] == "PROD|https://portal.azure.com/subscriptions/prod-id");
    CHECK(cfg.azure_subscription[1] == "NPRD|https://portal.azure.com/subscriptions/nprd-id");

    // Confirm both appear in the rendered notes file
    std::ifstream f((repo.path / ".projot" / "az7.md").string());
    std::string content((std::istreambuf_iterator<char>(f)), {});
    CHECK(content.find("[PROD](https://portal.azure.com/subscriptions/prod-id)") != std::string::npos);
    CHECK(content.find("[NPRD](https://portal.azure.com/subscriptions/nprd-id)") != std::string::npos);
}

// ── error path coverage (F008) ────────────────────────────────────────────────

TEST_CASE("add_todo_missing_text_fails") {
    TempRepo repo("add_todo_missing_text_fails");
    repo.init(); repo.new_project("e1");
    // No positional argument — should fail
    CHECK(cmd_add_todo(make_args("add-todo")) != 0);
}

TEST_CASE("complete_missing_todo_flag_fails") {
    TempRepo repo("complete_missing_todo_flag_fails");
    repo.init(); repo.new_project("e2");
    CHECK(cmd_complete(make_args("complete")) != 0);
}

TEST_CASE("complete_nonexistent_id_fails") {
    TempRepo repo("complete_nonexistent_id_fails");
    repo.init(); repo.new_project("e3");
    cmd_add_todo(make_args("add-todo", {}, "T"));
    // ID 99 does not exist
    CHECK(cmd_complete(make_args("complete", {{"todo", "99"}})) != 0);
}

TEST_CASE("complete_nonnumeric_id_fails") {
    TempRepo repo("complete_nonnumeric_id_fails");
    repo.init(); repo.new_project("e4");
    CHECK(cmd_complete(make_args("complete", {{"todo", "abc"}})) != 0);
}

TEST_CASE("add_note_missing_todo_flag_fails") {
    TempRepo repo("add_note_missing_todo_flag_fails");
    repo.init(); repo.new_project("e5");
    // No --todo flag
    CHECK(cmd_add_note(make_args("add-note", {}, "note text")) != 0);
}

TEST_CASE("add_note_missing_text_fails") {
    TempRepo repo("add_note_missing_text_fails");
    repo.init(); repo.new_project("e6");
    cmd_add_todo(make_args("add-todo", {}, "T"));
    // --todo provided but no positional note text
    CHECK(cmd_add_note(make_args("add-note", {{"todo", "1"}})) != 0);
}

TEST_CASE("add_note_nonexistent_id_fails") {
    TempRepo repo("add_note_nonexistent_id_fails");
    repo.init(); repo.new_project("e7");
    cmd_add_todo(make_args("add-todo", {}, "T"));
    CHECK(cmd_add_note(make_args("add-note", {{"todo", "99"}}, "note")) != 0);
}

TEST_CASE("set_link_missing_flags_fails") {
    TempRepo repo("set_link_missing_flags_fails");
    repo.init(); repo.new_project("e8");
    // Missing --url
    CHECK(cmd_set_link(make_args("set-link", {{"key", "teams"}})) != 0);
    // Missing --key
    CHECK(cmd_set_link(make_args("set-link", {{"url", "https://x.com"}})) != 0);
}

TEST_CASE("add_github_missing_url_fails") {
    TempRepo repo("add_github_missing_url_fails");
    repo.init(); repo.new_project("e9");
    CHECK(cmd_add_github(make_args("add-github")) != 0);
}

TEST_CASE("add_swagger_missing_url_fails") {
    TempRepo repo("add_swagger_missing_url_fails");
    repo.init(); repo.new_project("e10");
    CHECK(cmd_add_swagger(make_args("add-swagger")) != 0);
}

TEST_CASE("add_blizzard_missing_url_fails") {
    TempRepo repo("add_blizzard_missing_url_fails");
    repo.init(); repo.new_project("e11");
    CHECK(cmd_add_blizzard(make_args("add-blizzard")) != 0);
}

TEST_CASE("add_todo_fails_without_project") {
    TempRepo repo("add_todo_fails_without_project");
    repo.init();
    // new_project not called — no RPM set
    CHECK(cmd_add_todo(make_args("add-todo", {}, "T")) != 0);
}

TEST_CASE("add_note_fails_without_project") {
    TempRepo repo("add_note_fails_without_project");
    repo.init();
    CHECK(cmd_add_note(make_args("add-note", {{"todo", "1"}}, "note")) != 0);
}

TEST_CASE("complete_fails_without_project") {
    TempRepo repo("complete_fails_without_project");
    repo.init();
    CHECK(cmd_complete(make_args("complete", {{"todo", "1"}})) != 0);
}

TEST_CASE("add_todo_fails_if_notes_file_deleted") {
    TempRepo repo("add_todo_fails_if_notes_file_deleted");
    repo.init(); repo.new_project("e12");
    // Delete the notes file after project creation
    std::error_code ec;
    fs::remove(repo.path / ".projot" / "e12.md", ec);
    // execute_project_command parse_markdown should fail → return 1
    CHECK(cmd_add_todo(make_args("add-todo", {}, "T")) != 0);
}

// ── render ────────────────────────────────────────────────────────────────────

TEST_CASE("render_updates_file") {
    TempRepo repo("render_updates_file");
    repo.init(); repo.new_project("18");
    cmd_add_todo(make_args("add-todo", {}, "Render me"));
    // Manually corrupt the notes file
    {
        std::ofstream f((repo.path / ".projot" / "18.md").string());
        f << "corrupted content\n";
    }
    // render should rewrite from config + todo state
    CHECK(cmd_render(make_args("render")) == 0);
    Project proj;
    auto r = parse_markdown((repo.path / ".projot" / "18.md").string(), proj);
    CHECK(r.ok);
    CHECK(proj.name == "Test Project");
}

TEST_CASE("version_flag") {
    // Tested through CLI in main; here verify parse_args sets version_requested
    char arg0[] = "projot";
    char arg1[] = "--version";
    char* argv[] = {arg0, arg1, nullptr};
    Args a = parse_args(2, argv);
    CHECK(a.version_requested);
}

// ── cmd_render error paths ────────────────────────────────────────────────────

TEST_CASE("render_no_project_is_noop") {
    TempRepo repo("render_no_project_is_noop");
    repo.init(); // no new_project — rpm is empty
    CHECK(cmd_render(make_args("render")) == 0);  // no-op; hook must not block commits between projects
}

TEST_CASE("render_write_failure") {
#ifndef _WIN32
    if (getuid() == 0) return;
    TempRepo repo("render_write_failure");
    repo.init();
    repo.new_project();
    fs::path notes = repo.path / ".projot" / "12345.md";
    std::error_code ec;
    fs::permissions(notes, fs::perms::owner_read | fs::perms::group_read, ec);
    int ret = cmd_render(make_args("render"));
    fs::permissions(notes, fs::perms::owner_all, ec); // restore before TempRepo cleanup
    CHECK(ret != 0);
#endif
}

// ── cmd_new render failure ────────────────────────────────────────────────────

TEST_CASE("new_render_failure") {
#ifndef _WIN32
    if (getuid() == 0) return;
    TempRepo repo("new_render_failure");
    repo.init();
    // Pre-create the notes file as read-only so render_to_file fails inside cmd_new.
    fs::path notes = repo.path / ".projot" / "12345.md";
    { std::ofstream f(notes); }
    std::error_code ec;
    fs::permissions(notes, fs::perms::owner_read | fs::perms::group_read, ec);
    int ret = repo.new_project(); // write_config succeeds, then render_to_file fails
    fs::permissions(notes, fs::perms::owner_all, ec);
    CHECK(ret != 0);
#endif
}

// ── set-global + global config merge ──────────────────────────────────────────

// Redirects XDG_CONFIG_HOME (Linux/macOS) to an isolated temp dir for the
// duration of a test so cmd_set_global and load_context() use a throw-away path.
struct TempGlobalConfig {
    fs::path dir;
#ifdef _WIN32
    std::string prev_appdata;
    bool had_appdata = false;
#else
    std::string prev_xdg;
    bool had_prev = false;
#endif

    explicit TempGlobalConfig(const std::string& name) {
        dir = fs::temp_directory_path() / ("projot_global_" + name);
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
#ifdef _WIN32
        char* prev = nullptr;
        size_t len = 0;
        if (_dupenv_s(&prev, &len, "APPDATA") == 0 && prev) {
            had_appdata = true;
            prev_appdata = prev;
            free(prev);
        }
        _putenv_s("APPDATA", dir.string().c_str());
#else
        const char* prev = std::getenv("XDG_CONFIG_HOME");
        had_prev = (prev != nullptr);
        if (had_prev) prev_xdg = prev;
        setenv("XDG_CONFIG_HOME", dir.string().c_str(), 1);
#endif
    }

    ~TempGlobalConfig() {
#ifdef _WIN32
        if (had_appdata) {
            _putenv_s("APPDATA", prev_appdata.c_str());
        } else {
            _putenv_s("APPDATA", "");
        }
#else
        if (had_prev) setenv("XDG_CONFIG_HOME", prev_xdg.c_str(), 1);
        else          unsetenv("XDG_CONFIG_HOME");
#endif
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    fs::path config_path() const { return dir / "projot" / "config"; }
};

TEST_CASE("set_global_writes_rpm_base_url") {
    TempGlobalConfig global("set_global_rpm");
    int ret = cmd_set_global(make_args("set-global", {{"rpm-base-url", "https://rpm.example.com/"}}));
    CHECK(ret == 0);
    Config cfg;
    REQUIRE(parse_config(global.config_path().string(), cfg).ok);
    CHECK(cfg.rpm_base_url == "https://rpm.example.com/");
}

TEST_CASE("set_global_writes_itrack_base_url") {
    TempGlobalConfig global("set_global_itrack");
    int ret = cmd_set_global(make_args("set-global", {{"itrack-base-url", "https://itrack.example.com/"}}));
    CHECK(ret == 0);
    Config cfg;
    REQUIRE(parse_config(global.config_path().string(), cfg).ok);
    CHECK(cfg.itrack_base_url == "https://itrack.example.com/");
}

TEST_CASE("set_global_preserves_existing_value") {
    TempGlobalConfig global("set_global_preserve");
    // Set rpm first, then itrack — both should survive the second write.
    cmd_set_global(make_args("set-global", {{"rpm-base-url",    "https://rpm.example.com/"}}));
    cmd_set_global(make_args("set-global", {{"itrack-base-url", "https://itrack.example.com/"}}));
    Config cfg;
    REQUIRE(parse_config(global.config_path().string(), cfg).ok);
    CHECK(cfg.rpm_base_url    == "https://rpm.example.com/");
    CHECK(cfg.itrack_base_url == "https://itrack.example.com/");
}

TEST_CASE("set_global_requires_at_least_one_flag") {
    TempGlobalConfig global("set_global_no_flags");
    int ret = cmd_set_global(make_args("set-global"));
    CHECK(ret != 0);
}

TEST_CASE("load_context_merges_global_rpm_base_url") {
    TempGlobalConfig global("merge_rpm");
    // Write global config before TempRepo changes cwd.
    fs::create_directories(global.config_path().parent_path());
    { std::ofstream f(global.config_path()); f << "rpm_base_url = https://rpm.example.com/\n"; }

    TempRepo repo("merge_rpm");
    repo.init();
    repo.new_project();

    auto ctx = load_context();
    REQUIRE(ctx.ok);
    CHECK(ctx.config.rpm_base_url == "https://rpm.example.com/");
}

TEST_CASE("load_context_local_config_overrides_global") {
    TempGlobalConfig global("override_rpm");
    fs::create_directories(global.config_path().parent_path());
    { std::ofstream f(global.config_path()); f << "rpm_base_url = https://global.example.com/\n"; }

    TempRepo repo("override_rpm");
    repo.init();
    repo.new_project();
    // Append a local rpm_base_url that should win over the global one.
    { std::ofstream f(repo.path / ".projot" / "config", std::ios::app);
      f << "rpm_base_url = https://local.example.com/\n"; }

    auto ctx = load_context();
    REQUIRE(ctx.ok);
    CHECK(ctx.config.rpm_base_url == "https://local.example.com/");
}

// ── cmd_status ────────────────────────────────────────────────────────────────

TEST_CASE("status_sets_in_progress") {
    TempRepo repo("status_in_progress");
    repo.init(); repo.new_project("20");
    cmd_add_todo(make_args("add-todo", {}, "Work item"));
    int ret = cmd_status(make_args("status", {{"todo", "1"}}, "in-progress"));
    CHECK(ret == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "20.md").string(), proj);
    REQUIRE(!proj.todos.empty());
    CHECK(proj.todos[0].status == TodoStatus::InProgress);
}

TEST_CASE("status_sets_blocked") {
    TempRepo repo("status_blocked");
    repo.init(); repo.new_project("21");
    cmd_add_todo(make_args("add-todo", {}, "Blocked item"));
    int ret = cmd_status(make_args("status", {{"todo", "1"}}, "blocked"));
    CHECK(ret == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "21.md").string(), proj);
    REQUIRE(!proj.todos.empty());
    CHECK(proj.todos[0].status == TodoStatus::Blocked);
}

TEST_CASE("status_sets_done") {
    TempRepo repo("status_done");
    repo.init(); repo.new_project("22");
    cmd_add_todo(make_args("add-todo", {}, "Finish me"));
    int ret = cmd_status(make_args("status", {{"todo", "1"}}, "done"));
    CHECK(ret == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "22.md").string(), proj);
    REQUIRE(!proj.todos.empty());
    CHECK(proj.todos[0].status == TodoStatus::Done);
    CHECK(!proj.todos[0].completed_date.empty());
}

TEST_CASE("status_already_at_status_warns_not_errors") {
    TempRepo repo("status_already");
    repo.init(); repo.new_project("23");
    cmd_add_todo(make_args("add-todo", {}, "Todo item"));
    // todo is already in Todo state
    int ret = cmd_status(make_args("status", {{"todo", "1"}}, "todo"));
    CHECK(ret == 0); // warn, not error
}

TEST_CASE("status_unknown_status_string_errors") {
    TempRepo repo("status_unknown");
    repo.init(); repo.new_project("24");
    cmd_add_todo(make_args("add-todo", {}, "Todo item"));
    int ret = cmd_status(make_args("status", {{"todo", "1"}}, "invalid-status"));
    CHECK(ret != 0);
}

TEST_CASE("status_missing_todo_flag_errors") {
    TempRepo repo("status_no_todo");
    repo.init(); repo.new_project("25");
    int ret = cmd_status(make_args("status", {}, "in-progress"));
    CHECK(ret != 0);
}

TEST_CASE("status_complete_still_works_backward_compat") {
    TempRepo repo("status_complete_compat");
    repo.init(); repo.new_project("26");
    cmd_add_todo(make_args("add-todo", {}, "Compat todo"));
    CHECK(cmd_complete(make_args("complete", {{"todo", "1"}})) == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "26.md").string(), proj);
    REQUIRE(!proj.todos.empty());
    CHECK(proj.todos[0].status == TodoStatus::Done);
}
