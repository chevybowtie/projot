#include "markdown.h"
#include "config.h"  // for trim()

#include <fstream>
#include <sstream>
#include <algorithm>

// Line helpers

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

// Parser state machine

enum class Section {
    Header,
    Links,
    GitHub,
    Swagger,
    Blizzard,
    Todos,
    Unknown
};

// Parse a todo header line like "1. [ ] text", "2. [>] text", "3. [~] text", "4. [x] text".
// Returns true and fills id/status/text on success.
static bool parse_todo_line(const std::string& line, int& id, TodoStatus& status, std::string& text) {
    std::size_t dot = line.find(". ");
    if (dot == std::string::npos) return false;

    const std::string id_str = line.substr(0, dot);
    if (id_str.empty() || id_str.find_first_not_of("0123456789") != std::string::npos) return false;
    try {
        id = std::stoi(id_str);
    } catch (...) {
        return false;
    }

    // "[?]" then either end of line or " text". A bare "[ ]" is what an editor that
    // strips trailing whitespace leaves of a todo with empty text.
    const std::string rest = line.substr(dot + 2);
    if (rest.size() < 3 || rest[0] != '[' || rest[2] != ']') return false;
    if (rest.size() > 3 && rest[3] != ' ') return false;

    switch (rest[1]) {
        case ' ':           status = TodoStatus::Todo;       break;
        case '>':           status = TodoStatus::InProgress; break;
        case '~':           status = TodoStatus::Blocked;    break;
        case 'x': case 'X': status = TodoStatus::Done;       break;
        default:            return false;
    }
    text = rest.size() > 4 ? rest.substr(4) : "";
    return true;
}

// Core parsing logic

static MarkdownParseResult parse_lines(const std::vector<std::string>& lines, Project& out) {
    out = Project{};

    Section section = Section::Header;
    Todo* current_todo = nullptr;
    bool in_notes_block = false;

    std::size_t line_no = 0;
    for (const auto& raw : lines) {
        ++line_no;
        // Strip CRLF
        std::string line = raw;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Section transitions. The renderer emits "## Todos" last, so a heading after
        // it is user content (handled as an unparsed line below), not a new section.
        if (section != Section::Todos && starts_with(line, "## ")) {
            if (starts_with(line, "## Links"))         section = Section::Links;
            else if (starts_with(line, "## GitHub"))   section = Section::GitHub;
            else if (starts_with(line, "## Swagger"))  section = Section::Swagger;
            else if (starts_with(line, "## Blizzard")) section = Section::Blizzard;
            else if (starts_with(line, "## Todos")) {
                section = Section::Todos;
                out.has_todos_section = true;
            }
            else section = Section::Unknown;
            continue;
        }
        // Skip the projot-managed comment
        if (section != Section::Todos && starts_with(line, "<!-- projot-managed")) continue;

        // Header section
        if (section == Section::Header) {
            if (starts_with(line, "# Project: ")) {
                out.name = trim(line.substr(11));
            } else if (starts_with(line, "- RPM: ")) {
                out.rpm = trim(line.substr(7));
            } else if (starts_with(line, "- RANP: ")) {
                // Legacy: accept old "RANP" label for backward compatibility with existing notes files.
                out.rpm = trim(line.substr(8));
            } else if (starts_with(line, "- Jira: ")) {
                const auto v = trim(line.substr(8));
                out.jira = (v == "N/A") ? "" : v;
            } else if (starts_with(line, "- iTrack: ")) {
                // Legacy: accept old "iTrack" label for backward compatibility with existing notes files.
                const auto v = trim(line.substr(10));
                out.jira = (v == "N/A") ? "" : v;
            } else if (starts_with(line, "- App ID: ")) {
                const auto v = trim(line.substr(10));
                out.app_id = (v == "N/A") ? "" : v;
            } else if (starts_with(line, "- Created: ")) {
                out.created = trim(line.substr(11));
            }
            continue;
        }

        // Links section
        if (section == Section::Links) {
            if (starts_with(line, "- ") && line.find(": ") != std::string::npos) {
                const auto colon = line.find(": ");
                std::string key = trim(line.substr(2, colon - 2));
                std::string url = trim(line.substr(colon + 2));
                // Lowercase key for lookup
                std::string lower_key = key;
                std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                out.link_entries.emplace_back(lower_key, (url == "N/A") ? "" : url);
            }
            continue;
        }

        // Managed URL sections
        if (section == Section::GitHub) {
            if (starts_with(line, "- ")) {
                const auto url = trim(line.substr(2));
                if (!url.empty()) out.github_urls.push_back(url);
            }
            continue;
        }
        if (section == Section::Swagger) {
            if (starts_with(line, "- ")) {
                const auto url = trim(line.substr(2));
                if (!url.empty()) out.swagger_urls.push_back(url);
            }
            continue;
        }
        if (section == Section::Blizzard) {
            if (starts_with(line, "- ")) {
                const auto url = trim(line.substr(2));
                if (!url.empty()) out.blizzard_urls.push_back(url);
            }
            continue;
        }

        // Todos section
        if (section == Section::Todos) {
            const std::string trimmed = trim(line);
            if (trimmed.empty()) continue;

            // Try to parse a new todo header line: "1. [ ] text", "2. [>] text", etc.
            int id; TodoStatus status; std::string text;
            if (std::isdigit(static_cast<unsigned char>(line[0]))) {
                if (parse_todo_line(line, id, status, text)) {
                    out.todos.push_back(Todo{id, text, status, "", "", {}});
                    current_todo = &out.todos.back();
                    in_notes_block = false;
                    continue;
                }
                // Detail lines under an unreadable header must not attach to the
                // previous todo, overwriting its dates or merging notes into it.
                current_todo = nullptr;
                in_notes_block = false;
                out.unparsed_lines.emplace_back(line_no, line);
                continue;
            }

            // Note lines are told apart from detail lines by their deeper indent.
            // Detail lines match on trimmed text so a stripped trailing space after an
            // empty value ("   - Created:") still parses.
            if (current_todo && in_notes_block && starts_with(line, "     -")) {
                current_todo->notes.push_back(trim(line.substr(6)));
            } else if (current_todo && starts_with(trimmed, "- Created:")) {
                current_todo->created_date = trim(trimmed.substr(10));
            } else if (current_todo && starts_with(trimmed, "- Completed:")) {
                current_todo->completed_date = trim(trimmed.substr(12));
            } else if (current_todo && trimmed == "- Notes:") {
                in_notes_block = true;
            } else {
                out.unparsed_lines.emplace_back(line_no, line);
            }
            continue;
        }
    }

    return {true, ""};
}

// Public API

MarkdownParseResult parse_markdown(const std::string& path, Project& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {false, "Cannot open notes file: " + path};
    }
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return parse_lines(lines, out);
}

MarkdownParseResult parse_markdown_string(const std::string& content, Project& out) {
    std::istringstream ss(content);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    return parse_lines(lines, out);
}
