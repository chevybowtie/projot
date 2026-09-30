#include "doctest.h"
#include "markdown.h"
#include "todo.h"
#include <filesystem>

// ── Header parsing ────────────────────────────────────────────────────────────

TEST_CASE("parse_header_fields") {
    Project proj;
    auto result = parse_markdown(PROJOT_TEST_DATA_DIR "/notes/basic.md", proj);
    REQUIRE(result.ok);
    CHECK(proj.name == "Test Project");
    CHECK(proj.rpm == "12345");
    CHECK(proj.jira == "67890");
    CHECK(proj.app_id == "MyApp");
    CHECK(proj.created == "2025-11-23");
}

TEST_CASE("parse_header_na_values") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/no_todos.md", proj);
    CHECK(proj.jira.empty()); // N/A -> empty string
    CHECK(proj.app_id.empty()); // N/A -> empty string
}

TEST_CASE("parse_links_section") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/basic.md", proj);
    REQUIRE(proj.link_entries.size() == 3);
    CHECK(proj.link_entries[0].first == "teams");
    CHECK(proj.link_entries[0].second == "https://teams.microsoft.com/channel");
}

TEST_CASE("parse_github_section") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/managed_sections.md", proj);
    REQUIRE(proj.github_urls.size() == 2);
    CHECK(proj.github_urls[0] == "https://github.com/org/repo-one");
    CHECK(proj.github_urls[1] == "https://github.com/org/repo-two");
}

TEST_CASE("parse_swagger_section") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/managed_sections.md", proj);
    REQUIRE(proj.swagger_urls.size() == 1);
    CHECK(proj.swagger_urls[0] == "https://api.example.com/swagger");
}

TEST_CASE("parse_blizzard_section") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/managed_sections.md", proj);
    REQUIRE(proj.blizzard_urls.size() == 1);
    CHECK(proj.blizzard_urls[0] == "https://blizzard.example.com/project");
}

TEST_CASE("parse_missing_managed_sections") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/basic.md", proj);
    CHECK(proj.github_urls.empty());
    CHECK(proj.swagger_urls.empty());
    CHECK(proj.blizzard_urls.empty());
}

TEST_CASE("parse_jira_label_new") {
    const std::string content =
        "# Project: Jira Test\n\n"
        "- RPM: 1\n- Jira: 99999\n- App ID: N/A\n- Created: 2025-01-01\n\n"
        "## Links\n\n"
        "## Todos\n";
    Project proj;
    parse_markdown_string(content, proj);
    CHECK(proj.jira == "99999");
}

TEST_CASE("parse_itrack_label_legacy") {
    const std::string content =
        "# Project: Legacy iTrack\n\n"
        "- RPM: 1\n- iTrack: 88888\n- App ID: N/A\n- Created: 2025-01-01\n\n"
        "## Links\n\n"
        "## Todos\n";
    Project proj;
    parse_markdown_string(content, proj);
    CHECK(proj.jira == "88888");
}

// ── Todo parsing ──────────────────────────────────────────────────────────────

TEST_CASE("parse_todo_open") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/basic.md", proj);
    REQUIRE(proj.todos.size() >= 1);
    const auto& t = proj.todos[0];
    CHECK(t.id == 1);
    CHECK(t.status == TodoStatus::Todo);
    CHECK(t.text == "First open todo");
}

TEST_CASE("parse_todo_closed") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/basic.md", proj);
    REQUIRE(proj.todos.size() >= 2);
    const auto& t = proj.todos[1];
    CHECK(t.id == 2);
    CHECK(t.status == TodoStatus::Done);
}

TEST_CASE("parse_todo_created_date") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/basic.md", proj);
    CHECK(proj.todos[0].created_date == "2025-11-23");
}

TEST_CASE("parse_todo_completed_date") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/basic.md", proj);
    CHECK(proj.todos[1].completed_date == "2025-11-21");
}

TEST_CASE("parse_todo_notes") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/basic.md", proj);
    REQUIRE(proj.todos[0].notes.size() == 1);
    CHECK(proj.todos[0].notes[0] == "Waiting on feedback");
}

TEST_CASE("parse_todo_no_notes") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/multi_todo.md", proj);
    // Todo 1 has empty notes block
    REQUIRE(!proj.todos.empty());
    CHECK(proj.todos[0].notes.empty());
}

TEST_CASE("parse_multiple_todos") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/multi_todo.md", proj);
    CHECK(proj.todos.size() == 10);
    CHECK(proj.todos[9].id == 10);
}

TEST_CASE("parse_stable_ids_with_gap") {
    const std::string content =
        "# Project: Gap Project\n"
        "- RPM: 1\n- iTrack: N/A\n- App ID: N/A\n- Created: 2025-01-01\n\n"
        "## Links\n\n"
        "## Todos\n\n"
        "1. [ ] Todo one\n   - Created: 2025-01-01\n   - Notes:\n\n"
        "2. [ ] Todo two\n   - Created: 2025-01-02\n   - Notes:\n\n"
        "4. [ ] Todo four\n   - Created: 2025-01-03\n   - Notes:\n\n";
    Project proj;
    parse_markdown_string(content, proj);
    REQUIRE(proj.todos.size() == 3);
    CHECK(proj.todos[0].id == 1);
    CHECK(proj.todos[1].id == 2);
    CHECK(proj.todos[2].id == 4);
}

TEST_CASE("parse_crlf_in_notes_file") {
    const std::string content =
        "# Project: CRLF Project\r\n"
        "- RPM: 999\r\n"
        "- iTrack: N/A\r\n"
        "- App ID: N/A\r\n"
        "- Created: 2025-01-01\r\n"
        "\r\n"
        "## Links\r\n"
        "\r\n"
        "## Todos\r\n"
        "\r\n"
        "1. [ ] CRLF todo\r\n"
        "   - Created: 2025-01-01\r\n"
        "   - Notes:\r\n"
        "\r\n";
    Project proj;
    parse_markdown_string(content, proj);
    REQUIRE(proj.todos.size() == 1);
    CHECK(proj.todos[0].text == "CRLF todo");
    CHECK(proj.todos[0].text.back() != '\r');
    CHECK(proj.rpm == "999");
    CHECK(proj.rpm.back() != '\r');
}

TEST_CASE("parse_empty_todos_section") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/no_todos.md", proj);
    CHECK(proj.todos.empty());
}

TEST_CASE("parse_no_todos_section") {
    const std::string content =
        "# Project: No Section\n"
        "- RPM: 1\n- iTrack: N/A\n- App ID: N/A\n- Created: 2025-01-01\n\n"
        "## Links\n\n";
    Project proj;
    parse_markdown_string(content, proj);
    CHECK(proj.todos.empty());
}

TEST_CASE("parse_all_four_markers") {
    const std::string content =
        "# Project: Markers\n"
        "- RPM: 1\n- iTrack: N/A\n- App ID: N/A\n- Created: 2025-01-01\n\n"
        "## Links\n\n"
        "## Todos\n\n"
        "1. [ ] Todo item\n   - Created: 2025-01-01\n   - Notes:\n\n"
        "2. [>] In progress\n   - Created: 2025-01-02\n   - Notes:\n\n"
        "3. [~] Blocked item\n   - Created: 2025-01-03\n   - Notes:\n\n"
        "4. [x] Done item\n   - Created: 2025-01-04\n   - Notes:\n\n";
    Project proj;
    parse_markdown_string(content, proj);
    REQUIRE(proj.todos.size() == 4);
    CHECK(proj.todos[0].status == TodoStatus::Todo);
    CHECK(proj.todos[1].status == TodoStatus::InProgress);
    CHECK(proj.todos[2].status == TodoStatus::Blocked);
    CHECK(proj.todos[3].status == TodoStatus::Done);
}

TEST_CASE("parse_backward_compat_open_closed_only") {
    const std::string content =
        "# Project: OldFormat\n"
        "- RPM: 2\n- iTrack: N/A\n- App ID: N/A\n- Created: 2025-01-01\n\n"
        "## Links\n\n"
        "## Todos\n\n"
        "1. [ ] Still open\n   - Created: 2025-01-01\n   - Notes:\n\n"
        "2. [x] Already done\n   - Created: 2025-01-01\n   - Completed: 2025-01-05\n   - Notes:\n\n";
    Project proj;
    parse_markdown_string(content, proj);
    REQUIRE(proj.todos.size() == 2);
    CHECK(proj.todos[0].status == TodoStatus::Todo);
    CHECK(proj.todos[1].status == TodoStatus::Done);
    CHECK(proj.todos[1].completed_date == "2025-01-05");
}

// ── Lines the renderer would drop ─────────────────────────────────────────────

static const std::string kTodosPrefix =
    "# Project: P\n- RPM: 1\n- Jira: N/A\n- App ID: N/A\n- Created: 2025-01-01\n\n"
    "## Links\n\n"
    "## Todos\n\n";

TEST_CASE("parse_rendered_file_has_nothing_unparsed") {
    Project proj;
    parse_markdown(PROJOT_TEST_DATA_DIR "/notes/multi_todo.md", proj);
    CHECK(proj.has_todos_section);
    CHECK(proj.unparsed_lines.empty());
}

TEST_CASE("parse_uppercase_x_is_done") {
    Project proj;
    parse_markdown_string(kTodosPrefix + "1. [X] Done\n   - Created: 2025-01-01\n   - Notes:\n", proj);
    REQUIRE(proj.todos.size() == 1);
    CHECK(proj.todos[0].status == TodoStatus::Done);
    CHECK(proj.unparsed_lines.empty());
}

TEST_CASE("parse_whitespace_stripped_empty_values") {
    // An editor that strips trailing whitespace turns "[ ] " and "Created: " into these.
    Project proj;
    parse_markdown_string(kTodosPrefix + "1. [ ]\n   - Created:\n   - Notes:\n", proj);
    REQUIRE(proj.todos.size() == 1);
    CHECK(proj.todos[0].text.empty());
    CHECK(proj.todos[0].created_date.empty());
    CHECK(proj.unparsed_lines.empty());
}

TEST_CASE("parse_unreadable_header_does_not_merge_into_previous_todo") {
    Project proj;
    parse_markdown_string(kTodosPrefix +
        "1. [ ] First\n   - Created: 2025-01-01\n   - Notes:\n     - mine\n\n"
        "2. [?] Second\n   - Created: 2025-02-02\n   - Notes:\n     - theirs\n", proj);
    REQUIRE(proj.todos.size() == 1);
    CHECK(proj.todos[0].created_date == "2025-01-01");
    REQUIRE(proj.todos[0].notes.size() == 1);
    CHECK(proj.todos[0].notes[0] == "mine");
    REQUIRE(proj.unparsed_lines.size() == 4);
    CHECK(proj.unparsed_lines[0].first == 16);
    CHECK(proj.unparsed_lines[0].second == "2. [?] Second");
}

TEST_CASE("parse_records_free_text_and_headings_after_todos") {
    Project proj;
    parse_markdown_string(kTodosPrefix +
        "1. [ ] First\n   - Created: 2025-01-01\n   - Notes:\n"
        "   A paragraph I wrote\n\n## My section\n", proj);
    REQUIRE(proj.todos.size() == 1);
    REQUIRE(proj.unparsed_lines.size() == 2);
    CHECK(proj.unparsed_lines[0].second == "   A paragraph I wrote");
    CHECK(proj.unparsed_lines[1].second == "## My section");
}

TEST_CASE("parse_rejects_non_numeric_id_prefix") {
    Project proj;
    parse_markdown_string(kTodosPrefix + "1a. [ ] Odd\n", proj);
    CHECK(proj.todos.empty());
    CHECK(proj.unparsed_lines.size() == 1);
}

TEST_CASE("parse_missing_todos_heading") {
    Project proj;
    parse_markdown_string("# Project: P\n- RPM: 1\n\n1. [ ] Orphan\n", proj);
    CHECK(!proj.has_todos_section);
}

TEST_CASE("parse_rejects_ten_digit_id") {
    Project proj;
    parse_markdown_string(kTodosPrefix + "1000000000. [ ] Too big\n", proj);
    CHECK(proj.todos.empty());
    CHECK(proj.unparsed_lines.size() == 1);
}

TEST_CASE("parse_markdown_read_error_is_not_eof") {
#ifndef _WIN32
    auto dir = std::filesystem::temp_directory_path() / "projot_notes_is_dir";
    std::filesystem::create_directories(dir);
    Project proj;
    CHECK_FALSE(parse_markdown(dir.string(), proj).ok);
    std::filesystem::remove_all(dir);
#endif
}
