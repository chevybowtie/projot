#pragma once

#include "todo.h"
#include <string>
#include <vector>
#include <map>

// In-memory representation of a parsed .projot/{RANP}.md file.
struct Project {
    // Header fields
    std::string name;
    std::string ranp;
    std::string itrack;
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
};

struct MarkdownParseResult {
    bool ok = true;
    std::string error;
};

// Parse a .projot/{RANP}.md file into a Project.
MarkdownParseResult parse_markdown(const std::string& path, Project& out);

// Parse markdown from a string (useful for testing).
MarkdownParseResult parse_markdown_string(const std::string& content, Project& out);
