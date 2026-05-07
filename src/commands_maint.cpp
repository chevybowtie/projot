#include "commands.h"
#include "commands_internal.h"
#include "config.h"
#include "markdown.h"
#include "renderer.h"
#include "repo.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <optional>
#include <sstream>
#include <cstdlib>

#ifdef __APPLE__
#  include <mach-o/dyld.h>
#endif

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/wait.h>
#  include <fcntl.h>
#endif

namespace fs = std::filesystem;

// Validate that RPM is safe to embed in a shell command.
static bool is_safe_rpm(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
            return false;
    }
    return true;
}

// Stage a single file in the git index without invoking a shell.
// rel_path is relative to repo_root (e.g. ".projot/foo.md").
// Returns true on success; staging failure is non-fatal (best-effort).
static bool git_stage_file(const fs::path& repo_root, const std::string& rel_path) {
#ifdef _WIN32
    std::string cmd = "git -C \"" + repo_root.string() + "\" add \"" + rel_path + "\"";
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code == 0;
#else
    std::string repo = repo_root.string();
    const char* argv[] = {"git", "-C", repo.c_str(), "add", rel_path.c_str(), nullptr};
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0) { dup2(fd, STDERR_FILENO); close(fd); }
        execvp("git", const_cast<char**>(argv));
        _exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

// ── MCP helpers ───────────────────────────────────────────────────────────────

// Returns the path to the directory containing the running binary, or nullopt.
static std::optional<fs::path> binary_dir() {
#if defined(__linux__)
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return exe.parent_path();
#elif defined(__APPLE__)
    // Query required buffer size first, then allocate.
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buf(size, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) == 0) {
        std::error_code ec;
        fs::path exe = fs::canonical(buf.c_str(), ec);
        if (!ec) return exe.parent_path();
    }
#endif
    return std::nullopt;
}

// Searches for the bundled MCP directory relative to the binary location.
// Checks:
//   <bin_dir>/../mcp/                (source-tree or single-dir install)
//   <bin_dir>/../share/projot/mcp/   (standard install prefix layout)
static std::optional<fs::path> find_mcp_source_dir() {
    auto dir = binary_dir();
    if (!dir) return std::nullopt;

    for (const fs::path& raw : {
            *dir / ".." / "mcp",
            *dir / ".." / "share" / "projot" / "mcp"}) {
        std::error_code ec;
        fs::path candidate = fs::weakly_canonical(raw, ec);
        if (!ec && fs::exists(candidate / "server.js", ec))
            return candidate;
    }
    return std::nullopt;
}

// ── render ────────────────────────────────────────────────────────────────────

int cmd_render(const Args& args) {
    (void)args;

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    auto render = render_to_file(projot_file_path(ctx, ctx.config.rpm + ".md"), ctx.config, proj.todos);
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

    // Stage the rendered file. No shell involved; git_stage_file uses fork+execvp.
    if (is_safe_rpm(ctx.config.rpm)) {
        git_stage_file(ctx.repo_root, ".projot/" + ctx.config.rpm + ".md");
    }

    return 0;
}

// ── install-hook ──────────────────────────────────────────────────────────────

int cmd_install_hook(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot install-hook\n\n"
            "Install or re-install the pre-commit git hook.\n\n"
            "No flags required.\n\n"
            "Example:\n"
            "  projot install-hook\n";
        return 0;
    }

    auto root = find_repo_root();
    if (!root) {
        std::cerr << "error: not inside a git repository.\n";
        return 1;
    }

    bool appended = false;
    std::string error;
    if (!install_hook_impl(*root, appended, error)) {
        std::cerr << "warning: could not install pre-commit hook: " << error << "\n";
        return 1;
    }

    if (appended) {
        std::cout << "Note: appended projot render block to existing "
                     ".git/hooks/pre-commit\n";
    } else {
        std::cout << "Installed pre-commit hook at "
                  << (*root / ".git" / "hooks" / "pre-commit").string() << "\n";
    }
    return 0;
}

// ── install-mcp-server ────────────────────────────────────────────────────────

// Writes or updates .claude/settings.json with the projot MCP server config.
// Returns true on success or idempotent "already done" cases.
// Returns false on hard error (sets error string).
// If manual action is required, sets manual_action to the instruction text and returns true.
static bool install_claude_mcp(const fs::path& repo_root,
                                std::string& error,
                                std::string& manual_action) {
    fs::path claude_dir = repo_root / ".claude";
    fs::path settings_file = claude_dir / "settings.json";
    std::error_code ec;

    // Sentinel for "projot is already configured"
    const std::string projot_sentinel = "\"projot\"";

    // Case 1: File does not exist — create directory and write fresh config
    if (!fs::exists(settings_file, ec)) {
        fs::create_directories(claude_dir, ec);
        if (ec) { error = "cannot create .claude/: " + ec.message(); return false; }

        std::ofstream f(settings_file);
        if (!f.is_open()) { error = "cannot write .claude/settings.json"; return false; }

        f << "{\n"
          << "  \"mcpServers\": {\n"
          << "    \"projot\": {\n"
          << "      \"command\": \"node\",\n"
          << "      \"args\": [\"./mcp/server.js\"]\n"
          << "    }\n"
          << "  }\n"
          << "}\n";
        return true;
    }

    // Case 2+: File exists — check if already configured
    std::ifstream f_read(settings_file);
    if (!f_read.is_open()) { error = "cannot read .claude/settings.json"; return false; }

    std::string content((std::istreambuf_iterator<char>(f_read)),
                        std::istreambuf_iterator<char>());
    f_read.close();

    // Check if projot is already configured
    if (content.find(projot_sentinel) != std::string::npos) {
        // Already done — idempotent
        return true;
    }

    // Case 3: File has mcpServers but no projot entry — manual instruction
    if (content.find("\"mcpServers\"") != std::string::npos) {
        manual_action = "note: .claude/settings.json already has an mcpServers block.\n"
                       "Add the following entry to it manually:\n\n"
                       "    \"projot\": {\n"
                       "      \"command\": \"node\",\n"
                       "      \"args\": [\"./mcp/server.js\"]\n"
                       "    }\n";
        return true;
    }

    // Case 4: File has no mcpServers — inject the full block before the last }
    size_t last_brace = content.rfind('}');
    if (last_brace == std::string::npos) {
        error = ".claude/settings.json is malformed (no closing brace)";
        return false;
    }

    std::string to_inject = ",\n  \"mcpServers\": {\n"
                           "    \"projot\": {\n"
                           "      \"command\": \"node\",\n"
                           "      \"args\": [\"./mcp/server.js\"]\n"
                           "    }\n"
                           "  }";

    content.insert(last_brace, to_inject);

    std::ofstream f_write(settings_file);
    if (!f_write.is_open()) { error = "cannot write .claude/settings.json"; return false; }
    f_write << content;
    return true;
}

// Check if Node.js is available on PATH without spawning a shell.
static bool node_available() {
    const char* path_env = std::getenv("PATH");
    if (!path_env) return false;
    std::istringstream ss(path_env);
    std::string dir;
#ifdef _WIN32
    const char sep = ';';
    const std::string node_name = "node.exe";
#else
    const char sep = ':';
    const std::string node_name = "node";
#endif
    while (std::getline(ss, dir, sep)) {
        if (dir.empty()) continue;
        std::error_code ec;
        fs::path candidate = fs::path(dir) / node_name;
        if (fs::exists(candidate, ec) && !ec) return true;
    }
    return false;
}

int cmd_install_mcp_server(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot install-mcp-server [options]\n\n"
            "Configure the projot MCP server for use with Claude Code and VS Code.\n\n"
            "Writes .claude/settings.json (Claude Code) and .vscode/mcp.json (VS Code)\n"
            "with the server configuration. Skips any file that is already configured.\n\n"
            "Requires Node.js 16+ to be installed for the MCP server to run.\n\n"
            "Optional:\n"
            "  --no-vscode    Skip VS Code (.vscode/mcp.json) configuration\n\n"
            "Example:\n"
            "  projot install-mcp-server\n";
        return 0;
    }

    auto root = find_repo_root();
    if (!root) {
        std::cerr << "error: not inside a git repository.\n";
        return 1;
    }

    // Warn if Node.js is not available (non-fatal)
    if (!node_available()) {
        std::cerr << "warning: Node.js not found on PATH. MCP server requires Node.js 16+.\n"
                  << "         Install Node.js and try again, or see mcp/README.md for manual setup.\n";
    }

    // Ensure mcp/server.js exists in the repo
    fs::path mcp_in_repo = *root / "mcp" / "server.js";
    if (!fs::exists(mcp_in_repo)) {
        // Try to copy from install location
        auto mcp_src = find_mcp_source_dir();
        if (!mcp_src) {
            std::cerr << "error: mcp/server.js not found and cannot locate bundled MCP files.\n"
                      << "       ensure projot was installed correctly.\n";
            return 1;
        }

        fs::path mcp_dest = *root / "mcp";
        std::error_code ec;
        fs::create_directories(mcp_dest, ec);
        if (ec) {
            std::cerr << "error: cannot create mcp/: " << ec.message() << "\n";
            return 1;
        }

        for (const char* fname : {"server.js", "package.json"}) {
            fs::path src_file = *mcp_src / fname;
            if (!fs::exists(src_file, ec)) {
                std::cerr << "warning: expected MCP file not found: " << src_file.string() << "\n";
                continue;
            }
            fs::copy_file(src_file, mcp_dest / fname,
                         fs::copy_options::overwrite_existing, ec);
            if (ec) {
                std::cerr << "error: cannot copy " << fname << ": " << ec.message() << "\n";
                return 1;
            }
        }
    }

    // Configure .claude/settings.json
    std::string error, manual_action;
    if (!install_claude_mcp(*root, error, manual_action)) {
        std::cerr << "error: " << error << "\n";
        return 1;
    }

    if (!manual_action.empty()) {
        std::cout << manual_action;
    } else {
        std::cout << "Configured MCP server in .claude/settings.json\n";
    }

    // Configure .vscode/mcp.json unless --no-vscode
    if (!args.has("no-vscode")) {
        fs::path vscode_dir = *root / ".vscode";
        fs::path mcp_json = vscode_dir / "mcp.json";
        std::error_code ec;

        if (!fs::exists(mcp_json, ec)) {
            fs::create_directories(vscode_dir, ec);
            if (ec) {
                std::cerr << "error: cannot create .vscode/: " << ec.message() << "\n";
                return 1;
            }

            std::ofstream f(mcp_json);
            if (!f.is_open()) {
                std::cerr << "error: cannot write .vscode/mcp.json\n";
                return 1;
            }

            f << "{\n"
              << "  \"inputs\": [],\n"
              << "  \"servers\": {\n"
              << "    \"projot\": {\n"
              << "      \"type\": \"stdio\",\n"
              << "      \"command\": \"node\",\n"
              << "      \"args\": [\"./mcp/server.js\"]\n"
              << "    }\n"
              << "  }\n"
              << "}\n";

            std::cout << "Configured MCP server in .vscode/mcp.json\n";
        } else {
            std::cout << "VS Code MCP configuration already exists in .vscode/mcp.json\n";
        }
    }

    std::cout << "MCP server is ready. Reload your editor to enable tab completion and tools.\n";
    return 0;
}
