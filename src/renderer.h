#pragma once

#include "config.h"
#include "todo.h"
#include <string>
#include <vector>

struct RenderResult {
    bool ok = true;
    std::string error;
};

// Render a notes file to a string from config + todos.
// The config provides all metadata (app_id, github, swagger, blizzard, links, etc.).
// The todos list provides the todo state.
std::string render_markdown(const Config& cfg, const std::vector<Todo>& todos);

// Render and write to a file path.
RenderResult render_to_file(const std::string& path, const Config& cfg, const std::vector<Todo>& todos);
