#pragma once

#include "todo.h"
#include <string>
#include <vector>
#include <map>

// In-memory representation of a parsed .projot/{RPM}.md file.
struct Project {
    // Header fields
    std::string name;
    std::string rpm;
    std::string jira;
    std::string app_id;
    std::string created;

    // Links section: ordered list of (key, url) pairs as found in the file.
    // Keys match the values in Config::links; urls may be "N/A".
    std::vector<std::pair<std::string, std::string>> link_entries;

    // Projot-managed URL sections
    std::vector<std::string> github_urls;
    std::vector<std::string> swagger_urls;
    std::vector<std::string> blizzard_urls;

    // Todos
    std::vector<Todo> todos;

    // Everything from "## Todos" onward is regenerated from `todos` on render, so any
    // line there the parser could not place would be silently dropped. Commands must
    // refuse to rewrite the file while either of these reports a problem.
    bool has_todos_section = false;
    std::vector<std::pair<std::size_t, std::string>> unparsed_lines; // (1-based line no, text)
};

struct MarkdownParseResult {
    bool ok = true;
    std::string error;
};

// Parse a .projot/{RPM}.md file into a Project.
MarkdownParseResult parse_markdown(const std::string& path, Project& out);

// Parse markdown from a string (useful for testing).
MarkdownParseResult parse_markdown_string(const std::string& content, Project& out);
