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

// Parse a todo header line like "1. [ ] text" or "2. [x] text".
// Returns true and fills id/completed/text on success.
static bool parse_todo_line(const std::string& line, int& id, bool& completed, std::string& text) {
    // Expected format: "{id}. [x] {text}" or "{id}. [ ] {text}"
    std::size_t dot = line.find(". ");
    if (dot == std::string::npos) return false;

    try {
        id = std::stoi(line.substr(0, dot));
    } catch (...) {
        return false;
    }

    const std::string rest = line.substr(dot + 2);
    if (rest.size() < 4) return false;

    if (starts_with(rest, "[ ] ")) {
        completed = false;
        text = rest.substr(4);
    } else if (starts_with(rest, "[x] ")) {
        completed = true;
        text = rest.substr(4);
    } else {
        return false;
    }
    return true;
}

// Core parsing logic

static MarkdownParseResult parse_lines(const std::vector<std::string>& lines, Project& out) {
    out = Project{};

    Section section = Section::Header;
    Todo* current_todo = nullptr;
    bool in_notes_block = false;

    for (const auto& raw : lines) {
        // Strip CRLF
        std::string line = raw;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Section transitions
        if (starts_with(line, "## Links")) {
            section = Section::Links;
            current_todo = nullptr;
            in_notes_block = false;
            continue;
        }
        if (starts_with(line, "## GitHub")) {
            section = Section::GitHub;
            current_todo = nullptr;
            in_notes_block = false;
            continue;
        }
        if (starts_with(line, "## Swagger")) {
            section = Section::Swagger;
            current_todo = nullptr;
            in_notes_block = false;
            continue;
        }
        if (starts_with(line, "## Blizzard")) {
            section = Section::Blizzard;
            current_todo = nullptr;
            in_notes_block = false;
            continue;
        }
        if (starts_with(line, "## Todos")) {
            section = Section::Todos;
            current_todo = nullptr;
            in_notes_block = false;
            continue;
        }
        if (starts_with(line, "## ")) {
            section = Section::Unknown;
            current_todo = nullptr;
            in_notes_block = false;
            continue;
        }
        // Skip the projot-managed comment
        if (starts_with(line, "<!-- projot-managed")) continue;

        // Header section
        if (section == Section::Header) {
            if (starts_with(line, "# Project: ")) {
                out.name = trim(line.substr(11));
            } else if (starts_with(line, "- RPM: ")) {
                out.rpm = trim(line.substr(7));
            } else if (starts_with(line, "- RANP: ")) {
                // Legacy: accept old "RANP" label for backward compatibility with existing notes files.
                out.rpm = trim(line.substr(8));
            } else if (starts_with(line, "- iTrack: ")) {
                const auto v = trim(line.substr(10));
                out.itrack = (v == "N/A") ? "" : v;
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
            // Try to parse a new todo header line: "1. [ ] text"
            int id; bool completed; std::string text;
            if (!line.empty() && std::isdigit(static_cast<unsigned char>(line[0])) &&
                parse_todo_line(line, id, completed, text)) {
                out.todos.push_back(Todo{id, text, completed, "", "", {}});
                current_todo = &out.todos.back();
                in_notes_block = false;
                continue;
            }

            if (!current_todo) continue;

            if (starts_with(line, "   - Created: ")) {
                current_todo->created_date = trim(line.substr(14));
            } else if (starts_with(line, "   - Completed: ")) {
                current_todo->completed_date = trim(line.substr(16));
            } else if (trim(line) == "- Notes:") {
                in_notes_block = true;
            } else if (in_notes_block && starts_with(line, "     - ")) {
                current_todo->notes.push_back(trim(line.substr(7)));
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
