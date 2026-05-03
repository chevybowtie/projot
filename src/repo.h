#pragma once

#include <string>
#include <optional>
#include <filesystem>

// Walk up from 'start' (or current_path() if empty) looking for a .git entry.
// Returns the repo root path if found, or nullopt if not inside a git repo.
std::optional<std::filesystem::path> find_repo_root(
    const std::filesystem::path& start = std::filesystem::path{});
