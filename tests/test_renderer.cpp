#include "doctest.h"
#include "renderer.h"
#include "markdown.h"
#include "config.h"

#include <sstream>
#include <algorithm>

static Config make_base_config() {
    Config cfg;
    cfg.rpm = "12345";
    cfg.name = "Test Project";
    cfg.itrack = "67890";
    cfg.app_id = "MyApp";
    cfg.created = "2025-11-23";
    cfg.links = {"teams", "itrack"};
    cfg.labels["teams"] = "Teams";
    cfg.labels["itrack"] = "iTrack";
    cfg.link_urls["teams"] = "https://teams.microsoft.com/channel";
    cfg.link_urls["itrack"] = "https://itrack.example.com/67890";
    return cfg;
}

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// ── Header ────────────────────────────────────────────────────────────────────

TEST_CASE("render_header") {
    auto cfg = make_base_config();
    auto output = render_markdown(cfg, {});
    CHECK(contains(output, "# Project: Test Project"));
    CHECK(contains(output, "- RPM: 12345"));
    CHECK(contains(output, "- iTrack: 67890"));
    CHECK(contains(output, "- App ID: MyApp"));
    CHECK(contains(output, "- Created: 2025-11-23"));  // from cfg.created
}

// ── Links ─────────────────────────────────────────────────────────────────────

TEST_CASE("render_links_from_config") {
    auto cfg = make_base_config();
    auto output = render_markdown(cfg, {});
    CHECK(contains(output, "## Links"));
    CHECK(contains(output, "- Teams: https://teams.microsoft.com/channel"));
    CHECK(contains(output, "- iTrack: https://itrack.example.com/67890"));
}

TEST_CASE("render_links_na_when_missing") {
    auto cfg = make_base_config();
    cfg.links.push_back("rpm");
    cfg.labels["rpm"] = "RPM";
    // No link_urls["rpm"] set
    auto output = render_markdown(cfg, {});
    CHECK(contains(output, "- RPM: N/A"));
}

// ── Managed sections ──────────────────────────────────────────────────────────

TEST_CASE("render_github_section") {
    auto cfg = make_base_config();
    cfg.github = {"https://github.com/org/repo"};
    auto output = render_markdown(cfg, {});
    CHECK(contains(output, "## GitHub"));
    CHECK(contains(output, "- https://github.com/org/repo"));
}

TEST_CASE("render_swagger_section") {
    auto cfg = make_base_config();
    cfg.swagger = {"https://api.example.com/swagger"};
    auto output = render_markdown(cfg, {});
    CHECK(contains(output, "## Swagger"));
    CHECK(contains(output, "- https://api.example.com/swagger"));
}

TEST_CASE("render_blizzard_section") {
    auto cfg = make_base_config();
    cfg.blizzard = {"https://blizzard.example.com/project"};
    auto output = render_markdown(cfg, {});
    CHECK(contains(output, "## Blizzard"));
    CHECK(contains(output, "- https://blizzard.example.com/project"));
}

TEST_CASE("render_omits_empty_github") {
    auto cfg = make_base_config();
    cfg.github.clear();
    auto output = render_markdown(cfg, {});
    CHECK_FALSE(contains(output, "## GitHub"));
}

TEST_CASE("render_omits_empty_swagger") {
    auto cfg = make_base_config();
    cfg.swagger.clear();
    auto output = render_markdown(cfg, {});
    CHECK_FALSE(contains(output, "## Swagger"));
}

TEST_CASE("render_omits_empty_blizzard") {
    auto cfg = make_base_config();
    cfg.blizzard.clear();
    auto output = render_markdown(cfg, {});
    CHECK_FALSE(contains(output, "## Blizzard"));
}

TEST_CASE("render_managed_comment") {
    auto cfg = make_base_config();
    cfg.github = {"https://github.com/org/repo"};
    auto output = render_markdown(cfg, {});
    CHECK(contains(output, "<!-- projot-managed"));
}

TEST_CASE("render_section_order") {
    auto cfg = make_base_config();
    cfg.github = {"https://github.com/g"};
    cfg.swagger = {"https://swagger.com/s"};
    cfg.blizzard = {"https://blizzard.com/b"};
    auto output = render_markdown(cfg, {});
    auto links_pos   = output.find("## Links");
    auto github_pos  = output.find("## GitHub");
    auto swagger_pos = output.find("## Swagger");
    auto blizzard_pos= output.find("## Blizzard");
    auto todos_pos   = output.find("## Todos");
    CHECK(links_pos    < github_pos);
    CHECK(github_pos   < swagger_pos);
    CHECK(swagger_pos  < blizzard_pos);
    CHECK(blizzard_pos < todos_pos);
}

// ── Todos ─────────────────────────────────────────────────────────────────────

TEST_CASE("render_todo_open") {
    auto cfg = make_base_config();
    std::vector<Todo> todos = {{1, "My open todo", false, "2025-01-01", "", {}}};
    auto output = render_markdown(cfg, todos);
    CHECK(contains(output, "1. [ ] My open todo"));
    CHECK(contains(output, "   - Created: 2025-01-01"));
}

TEST_CASE("render_todo_closed") {
    auto cfg = make_base_config();
    std::vector<Todo> todos = {{1, "Done todo", true, "2025-01-01", "2025-01-05", {}}};
    auto output = render_markdown(cfg, todos);
    CHECK(contains(output, "1. [x] Done todo"));
    CHECK(contains(output, "   - Completed: 2025-01-05"));
}

TEST_CASE("render_todo_notes") {
    auto cfg = make_base_config();
    std::vector<Todo> todos = {{1, "Noted todo", false, "2025-01-01", "", {"Note one", "Note two"}}};
    auto output = render_markdown(cfg, todos);
    CHECK(contains(output, "     - Note one"));
    CHECK(contains(output, "     - Note two"));
}

TEST_CASE("render_todo_no_notes_block") {
    auto cfg = make_base_config();
    std::vector<Todo> todos = {{1, "No notes", false, "2025-01-01", "", {}}};
    auto output = render_markdown(cfg, todos);
    CHECK(contains(output, "   - Notes:"));
}

TEST_CASE("render_deduplicates_github_urls") {
    auto cfg = make_base_config();
    cfg.github = {"https://github.com/org/repo", "https://github.com/org/repo"};
    auto output = render_markdown(cfg, {});
    // Count occurrences
    std::size_t count = 0;
    std::size_t pos = 0;
    const std::string needle = "https://github.com/org/repo";
    while ((pos = output.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    CHECK(count == 1);
}

TEST_CASE("render_deduplicates_swagger_urls") {
    auto cfg = make_base_config();
    cfg.swagger = {"https://api.com/s", "https://api.com/s"};
    auto output = render_markdown(cfg, {});
    std::size_t count = 0, pos = 0;
    const std::string needle = "https://api.com/s";
    while ((pos = output.find(needle, pos)) != std::string::npos) { ++count; pos += needle.size(); }
    CHECK(count == 1);
}

TEST_CASE("render_lf_line_endings") {
    auto cfg = make_base_config();
    auto output = render_markdown(cfg, {});
    CHECK_FALSE(contains(output, "\r\n"));
    CHECK_FALSE(contains(output, "\r"));
}

TEST_CASE("render_round_trip") {
    auto cfg = make_base_config();
    cfg.github = {"https://github.com/org/repo"};
    std::vector<Todo> todos = {
        {1, "Open todo", false, "2025-01-01", "", {"note one"}},
        {2, "Closed todo", true, "2025-01-01", "2025-01-02", {}}
    };
    const auto output = render_markdown(cfg, todos);
    Project proj;
    auto result = parse_markdown_string(output, proj);
    REQUIRE(result.ok);
    CHECK(proj.name == cfg.name);
    CHECK(proj.rpm == cfg.rpm);
    REQUIRE(proj.todos.size() == 2);
    CHECK(proj.todos[0].id == 1);
    CHECK(proj.todos[0].text == "Open todo");
    CHECK_FALSE(proj.todos[0].completed);
    CHECK(proj.todos[0].notes[0] == "note one");
    CHECK(proj.todos[1].completed);
    CHECK(proj.todos[1].completed_date == "2025-01-02");
}

TEST_CASE("render_app_id_from_config") {
    auto cfg = make_base_config();
    cfg.app_id = "OverrideApp";
    auto output = render_markdown(cfg, {});
    CHECK(contains(output, "- App ID: OverrideApp"));
}

TEST_CASE("render_app_id_na") {
    auto cfg = make_base_config();
    cfg.app_id = "";
    auto output = render_markdown(cfg, {});
    CHECK(contains(output, "- App ID: N/A"));
}
