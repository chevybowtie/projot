#include "doctest.h"
#include "commands.h"
#include "config.h"
#include "markdown.h"

#include <filesystem>
#include <fstream>
#include <sstream>

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
                      std::initializer_list<std::pair<std::string,std::string>> flags = {}) {
    Args a;
    a.subcommand = sub;
    for (const auto& [k, v] : flags) a.flags[k].push_back(v);
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

// ── add-todo ──────────────────────────────────────────────────────────────────

TEST_CASE("add_todo_appends") {
    TempRepo repo("add_todo_appends");
    repo.init(); repo.new_project("1");
    Args a = make_args("add-todo", {{"text", "My first todo"}});
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
    cmd_add_todo(make_args("add-todo", {{"text", "First"}}));
    cmd_add_todo(make_args("add-todo", {{"text", "Second"}}));
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
    cmd_add_todo(make_args("add-todo", {{"text", "Open one"}}));
    cmd_add_todo(make_args("add-todo", {{"text", "Open two"}}));
    cmd_complete(make_args("complete", {{"todo", "1"}}));
    // Just check it returns 0 (output goes to stdout which we can't easily capture here)
    CHECK(cmd_list(make_args("list")) == 0);
}

TEST_CASE("list_closed_flag") {
    TempRepo repo("list_closed_flag");
    repo.init(); repo.new_project("4");
    cmd_add_todo(make_args("add-todo", {{"text", "T"}}));
    cmd_complete(make_args("complete", {{"todo", "1"}}));
    CHECK(cmd_list(make_args("list", {{"closed", "true"}})) == 0);
}

TEST_CASE("list_all_flag") {
    TempRepo repo("list_all_flag");
    repo.init(); repo.new_project("5");
    cmd_add_todo(make_args("add-todo", {{"text", "T"}}));
    CHECK(cmd_list(make_args("list", {{"all", "true"}})) == 0);
}

// ── complete ─────────────────────────────────────────────────────────────────

TEST_CASE("complete_marks_done") {
    TempRepo repo("complete_marks_done");
    repo.init(); repo.new_project("6");
    cmd_add_todo(make_args("add-todo", {{"text", "Todo"}}));
    CHECK(cmd_complete(make_args("complete", {{"todo", "1"}})) == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "6.md").string(), proj);
    REQUIRE(!proj.todos.empty());
    CHECK(proj.todos[0].completed);
    CHECK(!proj.todos[0].completed_date.empty());
}

TEST_CASE("complete_warns_if_already_done") {
    TempRepo repo("complete_warns_if_already_done");
    repo.init(); repo.new_project("7");
    cmd_add_todo(make_args("add-todo", {{"text", "T"}}));
    cmd_complete(make_args("complete", {{"todo", "1"}}));
    // Second complete should warn but return 0
    CHECK(cmd_complete(make_args("complete", {{"todo", "1"}})) == 0);
}

// ── add-note ─────────────────────────────────────────────────────────────────

TEST_CASE("add_note_appends") {
    TempRepo repo("add_note_appends");
    repo.init(); repo.new_project("8");
    cmd_add_todo(make_args("add-todo", {{"text", "T"}}));
    CHECK(cmd_add_note(make_args("add-note", {{"todo", "1"}, {"text", "My note"}})) == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "8.md").string(), proj);
    REQUIRE(!proj.todos.empty());
    REQUIRE(proj.todos[0].notes.size() == 1);
    CHECK(proj.todos[0].notes[0] == "My note");
}

TEST_CASE("add_note_warns_if_closed") {
    TempRepo repo("add_note_warns_if_closed");
    repo.init(); repo.new_project("9");
    cmd_add_todo(make_args("add-todo", {{"text", "T"}}));
    cmd_complete(make_args("complete", {{"todo", "1"}}));
    // Should warn but still return 0 and write note
    CHECK(cmd_add_note(make_args("add-note", {{"todo", "1"}, {"text", "Late"}})) == 0);
    Project proj;
    parse_markdown((repo.path / ".projot" / "9.md").string(), proj);
    CHECK(!proj.todos[0].notes.empty());
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

// ── render ────────────────────────────────────────────────────────────────────

TEST_CASE("render_updates_file") {
    TempRepo repo("render_updates_file");
    repo.init(); repo.new_project("18");
    cmd_add_todo(make_args("add-todo", {{"text", "Render me"}}));
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
