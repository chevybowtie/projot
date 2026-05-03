#include "repo.h"

std::optional<std::filesystem::path> find_repo_root(const std::filesystem::path& start) {
    namespace fs = std::filesystem;

    fs::path current = start.empty() ? fs::current_path() : fs::absolute(start);

    while (true) {
        // Accept both a .git directory and a .git file (worktrees)
        if (fs::exists(current / ".git")) {
            return current;
        }

        const auto parent = current.parent_path();
        if (parent == current) {
            // Reached filesystem root without finding .git
            return std::nullopt;
        }
        current = parent;
    }
}
