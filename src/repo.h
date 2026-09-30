#pragma once

#include <string>
#include <optional>
#include <filesystem>
#include <memory>

// Walk up from 'start' (or current_path() if empty) looking for a .git entry.
// Returns the repo root path if found, or nullopt if not inside a git repo.
std::optional<std::filesystem::path> find_repo_root(
    const std::filesystem::path& start = std::filesystem::path{});

// Returns the global config path for the current platform, or nullopt if not found.
// Windows: %APPDATA%\projot\config, fallback to %USERPROFILE%\.config\projot\config
// Unix/macOS/Linux: $XDG_CONFIG_HOME/projot/config, fallback to $HOME/.config/projot/config
std::optional<std::filesystem::path> global_config_path();

// The directory git keeps this checkout's metadata in: `<root>/.git` itself, or the
// target of a `gitdir:` pointer file (worktrees, submodules). nullopt if unreadable.
std::optional<std::filesystem::path> resolve_git_dir(const std::filesystem::path& repo_root);

// The directory git runs hooks from. Asks git first, which honours core.hooksPath
// (husky, lefthook) and worktree layouts; falls back to resolving the git dir by hand.
std::filesystem::path resolve_hooks_dir(const std::filesystem::path& repo_root);

// Exclusive per-checkout lock serialising projot commands that read-modify-write
// .projot/ files (e.g. the CLI racing the MCP server or the pre-commit hook). Held
// until the last shared_ptr is released. The OS drops it if the process dies, so a
// crash never leaves a stale lock. Re-acquiring in the same process shares the lock.
class RepoLock;
std::shared_ptr<RepoLock> acquire_repo_lock(const std::filesystem::path& repo_root,
                                            std::string& error);
