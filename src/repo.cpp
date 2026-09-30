#include "repo.h"
#include "config.h"   // trim()
#include "process.h"
#include "utils.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <cerrno>
#  include <fcntl.h>
#  include <sys/file.h>
#  include <time.h>
#  include <unistd.h>
#endif

namespace {

// First line of a small metadata file, trimmed; "" if unreadable.
std::string read_first_line(const std::filesystem::path& p) {
    std::ifstream f(p);
    std::string line;
    if (!f.is_open() || !std::getline(f, line)) return "";
    return trim(line);
}

constexpr int LOCK_TIMEOUT_MS = 10000;
constexpr int LOCK_RETRY_MS   = 100;

} // namespace

class RepoLock {
public:
    std::filesystem::path path;
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~RepoLock() {
        if (handle == INVALID_HANDLE_VALUE) return;
        OVERLAPPED ov{};
        UnlockFileEx(handle, 0, 1, 0, &ov);
        CloseHandle(handle);
    }
#else
    int fd = -1;
    ~RepoLock() { if (fd >= 0) close(fd); }  // closing releases the flock
#endif
};

std::optional<std::filesystem::path> resolve_git_dir(const std::filesystem::path& repo_root) {
    namespace fs = std::filesystem;
    const fs::path dot_git = repo_root / ".git";
    std::error_code ec;
    if (fs::is_directory(dot_git, ec)) return dot_git;

    const std::string line = read_first_line(dot_git);
    const std::string prefix = "gitdir:";
    if (line.rfind(prefix, 0) != 0) return std::nullopt;
    fs::path dir = trim(line.substr(prefix.size()));
    if (dir.empty()) return std::nullopt;
    if (dir.is_relative()) dir = repo_root / dir;
    return dir.lexically_normal();
}

std::filesystem::path resolve_hooks_dir(const std::filesystem::path& repo_root) {
    namespace fs = std::filesystem;
    auto r = run_process({"git", "-C", repo_root.string(), "rev-parse", "--git-path", "hooks"},
                         ChildOutput::Capture, ChildOutput::Discard);
    if (r.started && !r.timed_out && r.exit_code == 0) {
        const std::string out = trim(r.output);
        if (!out.empty() && !has_line_break(out)) {
            fs::path p(out);
            if (p.is_relative()) p = repo_root / p;
            return p.lexically_normal();
        }
    }

    // No usable git: hooks live in the common dir, which a worktree names in `commondir`.
    auto git_dir = resolve_git_dir(repo_root);
    if (!git_dir) return repo_root / ".git" / "hooks";
    const std::string common = read_first_line(*git_dir / "commondir");
    if (!common.empty()) {
        fs::path c(common);
        if (c.is_relative()) c = *git_dir / c;
        return (c / "hooks").lexically_normal();
    }
    return *git_dir / "hooks";
}

std::shared_ptr<RepoLock> acquire_repo_lock(const std::filesystem::path& repo_root,
                                            std::string& error) {
    // Nested acquisition within one process must share the lock: a second flock()
    // on a fresh descriptor would block against our own first one.
    static std::weak_ptr<RepoLock> held;

    // The lock lives in the git dir so it never shows up as an untracked file.
    auto git_dir = resolve_git_dir(repo_root);
    if (!git_dir) {
        error = "cannot locate the git directory for " + repo_root.string();
        return nullptr;
    }
    const std::filesystem::path lock_path = *git_dir / "projot.lock";
    if (auto existing = held.lock()) {
        if (existing->path == lock_path) return existing;
    }

    auto lock = std::make_shared<RepoLock>();
    lock->path = lock_path;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(LOCK_TIMEOUT_MS);

#ifdef _WIN32
    lock->handle = CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock->handle == INVALID_HANDLE_VALUE) {
        error = "cannot open lock file " + lock_path.string();
        return nullptr;
    }
    for (;;) {
        OVERLAPPED ov{};
        if (LockFileEx(lock->handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                       0, 1, 0, &ov)) break;
        if (GetLastError() != ERROR_LOCK_VIOLATION || std::chrono::steady_clock::now() >= deadline) {
            CloseHandle(lock->handle);
            lock->handle = INVALID_HANDLE_VALUE;
            error = "another projot command is running in this repository (lock: " +
                    lock_path.string() + ")";
            return nullptr;
        }
        Sleep(LOCK_RETRY_MS);
    }
#else
    lock->fd = open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (lock->fd < 0) {
        error = "cannot open lock file " + lock_path.string();
        return nullptr;
    }
    for (;;) {
        if (flock(lock->fd, LOCK_EX | LOCK_NB) == 0) break;
        if (errno == EINTR) continue;
        if (errno != EWOULDBLOCK || std::chrono::steady_clock::now() >= deadline) {
            error = "another projot command is running in this repository (lock: " +
                    lock_path.string() + ")";
            return nullptr;  // ~RepoLock closes the descriptor
        }
        struct timespec ts{0, LOCK_RETRY_MS * 1000L * 1000L};
        nanosleep(&ts, nullptr);
    }
#endif

    held = lock;
    return lock;
}

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

std::optional<std::filesystem::path> global_config_path() {
#ifdef _WIN32
    // Windows, use APPDATA or USERPROFILE
    auto get_env = [](const char* name) -> std::optional<std::string> {
        char* value = nullptr;
        size_t len = 0;
        if (_dupenv_s(&value, &len, name) == 0 && value != nullptr) {
            std::string result(value);
            free(value);
            if (!result.empty()) return result;
        }
        return std::nullopt;
    };

    if (auto appdata = get_env("APPDATA"))
        return std::filesystem::path(*appdata) / "projot" / "config";

    if (auto userprofile = get_env("USERPROFILE"))
        return std::filesystem::path(*userprofile) / ".config" / "projot" / "config";
#else
    // Linux and macOS, follow XDG Base Directory Specification
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        if (*xdg) return std::filesystem::path(xdg) / "projot" / "config";

    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".config" / "projot" / "config";
#endif

    return std::nullopt;
}
