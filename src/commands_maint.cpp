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
#include <vector>

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

// MCP helpers

// Returns the path to the directory containing the running binary, or nullopt.
static std::optional<fs::path> binary_dir() {
#if defined(_WIN32)
    constexpr size_t kMaxWindowsPathChars = 32768; // Extended-length Windows path limit.
    std::string buf(MAX_PATH, '\0');
    for (;;) {
        DWORD len = GetModuleFileNameA(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (len == 0) return std::nullopt;
        if (len < buf.size() - 1) {
            std::error_code ec;
            fs::path exe = fs::weakly_canonical(buf.substr(0, len), ec);
            if (!ec) return exe.parent_path();
            // Fall back to the raw module path if canonicalization fails.
            return fs::path(buf.substr(0, len)).parent_path();
        }
        if (buf.size() >= kMaxWindowsPathChars) return std::nullopt;
        buf.resize(buf.size() * 2);
    }
#elif defined(__linux__)
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
            *dir / ".." / "share" / "projot" / "mcp",
            *dir / ".." / ".." / "mcp",
            *dir / ".." / ".." / "share" / "projot" / "mcp"}) {
        std::error_code ec;
        fs::path candidate = fs::weakly_canonical(raw, ec);
        if (!ec && fs::exists(candidate / "server.js", ec))
            return candidate;
    }
    return std::nullopt;
}

static std::string json_escape(const std::string& s) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += hex[(c >> 4) & 0x0F];
                    out += hex[c & 0x0F];
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return out;
}

static std::string claude_settings_fresh(const std::string& server_arg) {
    return
        "{\n"
        "  \"mcpServers\": {\n"
        "    \"projot\": {\n"
        "      \"command\": \"node\",\n"
        "      \"args\": [\"" + server_arg + "\"]\n"
        "    }\n"
        "  }\n"
        "}\n";
}

static std::string claude_settings_injected_block(const std::string& server_arg) {
    return
        ",\n  \"mcpServers\": {\n"
        "    \"projot\": {\n"
        "      \"command\": \"node\",\n"
        "      \"args\": [\"" + server_arg + "\"]\n"
        "    }\n"
        "  }";
}

// render

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

// install-hook

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

// install-mcp-server

// Writes or updates .claude/settings.json with the projot MCP server config.
// Returns true on success or idempotent "already done" cases.
// Returns false on hard error (sets error string).
// If manual action is required, sets manual_action to the instruction text and returns true.
static bool install_claude_mcp(const fs::path& repo_root,
                                const std::string& server_arg,
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

        f << claude_settings_fresh(server_arg);
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
                       "      \"args\": [\"" + server_arg + "\"]\n"
                       "    }\n";
        return true;
    }

    // Case 4: File has no mcpServers — inject the full block before the last }
    size_t last_brace = content.rfind('}');
    if (last_brace == std::string::npos) {
        error = ".claude/settings.json is malformed (no closing brace)";
        return false;
    }

    std::string to_inject = claude_settings_injected_block(server_arg);

    content.insert(last_brace, to_inject);

    std::ofstream f_write(settings_file);
    if (!f_write.is_open()) { error = "cannot write .claude/settings.json"; return false; }
    f_write << content;
    return true;
}

// Check if Node.js is available on PATH without spawning a shell.
static bool node_available() {
#ifdef _WIN32
    char* path_env = nullptr;
    size_t len = 0;
    if (_dupenv_s(&path_env, &len, "PATH") != 0 || !path_env) return false;
#else
    const char* path_env = std::getenv("PATH");
    if (!path_env) return false;
#endif

    std::istringstream ss(path_env);
    std::string dir;
#ifdef _WIN32
    const char sep = ';';
    const std::string node_name = "node.exe";
#else
    const char sep = ':';
    const std::string node_name = "node";
#endif

    bool found = false;
    while (std::getline(ss, dir, sep)) {
        if (dir.empty()) continue;
        std::error_code ec;
        fs::path candidate = fs::path(dir) / node_name;
        if (fs::exists(candidate, ec) && !ec) {
            found = true;
            break;
        }
    }

#ifdef _WIN32
    free(path_env);
#endif
    return found;
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

    auto mcp_src = find_mcp_source_dir();
    if (!mcp_src) {
        std::cerr << "error: cannot locate bundled MCP files.\n"
                  << "       ensure projot was installed correctly.\n";
        return 1;
    }
    std::string server_arg = json_escape((*mcp_src / "server.js").string());

    // Configure .claude/settings.json
    std::string error, manual_action;
    if (!install_claude_mcp(*root, server_arg, error, manual_action)) {
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
              << "      \"args\": [\"" << server_arg << "\"]\n"
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

// uninstall-hook

int cmd_uninstall_hook(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot uninstall-hook\n\n"
            "Remove the projot block from the pre-commit git hook.\n\n"
            "If the hook file contains only the projot block it is deleted entirely.\n"
            "If other hook content is present, only the projot block is removed.\n\n"
            "Example:\n"
            "  projot uninstall-hook\n";
        return 0;
    }

    auto root = find_repo_root();
    if (!root) {
        std::cerr << "error: not inside a git repository.\n";
        return 1;
    }

    auto hook_path = *root / ".git" / "hooks" / "pre-commit";
    std::error_code ec;

    if (!fs::exists(hook_path, ec)) {
        std::cout << "No pre-commit hook installed.\n";
        return 0;
    }

    std::ifstream in(hook_path);
    if (!in.is_open()) {
        std::cerr << "error: cannot read " << hook_path.string() << "\n";
        return 1;
    }
    std::string content((std::istreambuf_iterator<char>(in)), {});
    in.close();

    if (content.find("projot render") == std::string::npos) {
        std::cout << "projot hook block not found in pre-commit hook.\n";
        return 0;
    }

    // Remove block with preceding newline (appended-to-existing case)
    std::string to_remove = "\n" + HOOK_BLOCK;
    auto pos = content.find(to_remove);
    if (pos != std::string::npos) {
        content.erase(pos, to_remove.size());
    } else {
        // Remove block without preceding newline (written-fresh case)
        pos = content.find(HOOK_BLOCK);
        if (pos != std::string::npos)
            content.erase(pos, HOOK_BLOCK.size());
    }

    // Check whether only a shebang line (or nothing) remains
    std::string remainder = content;
    if (remainder.compare(0, 2, "#!") == 0) {
        auto nl = remainder.find('\n');
        remainder = (nl != std::string::npos) ? remainder.substr(nl + 1) : "";
    }
    bool only_shebang = (remainder.find_first_not_of(" \t\n\r") == std::string::npos);

    if (only_shebang) {
        fs::remove(hook_path, ec);
        if (ec) {
            std::cerr << "error: cannot remove " << hook_path.string()
                      << ": " << ec.message() << "\n";
            return 1;
        }
        std::cout << "Removed pre-commit hook at " << hook_path.string() << "\n";
    } else {
        std::ofstream f(hook_path);
        if (!f.is_open()) {
            std::cerr << "error: cannot write " << hook_path.string() << "\n";
            return 1;
        }
        f << content;
        f.close();
#ifndef _WIN32
        fs::permissions(hook_path,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add, ec);
#endif
        std::cout << "Removed projot block from " << hook_path.string() << "\n";
    }
    return 0;
}

// uninstall-mcp-server

static bool uninstall_claude_mcp(const fs::path& repo_root, std::string& message) {
    fs::path settings_file = repo_root / ".claude" / "settings.json";
    std::error_code ec;

    if (!fs::exists(settings_file, ec)) {
        message = "No .claude/settings.json found.";
        return true;
    }

    std::ifstream f_read(settings_file);
    if (!f_read.is_open()) {
        message = "cannot read .claude/settings.json";
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(f_read)), {});
    f_read.close();

    if (content.find("\"projot\"") == std::string::npos) {
        message = "projot not configured in .claude/settings.json.";
        return true;
    }

    const std::string legacy_server_arg = "./mcp/server.js";
    std::vector<std::string> server_args{legacy_server_arg};
    if (auto mcp_src = find_mcp_source_dir()) {
        server_args.push_back(json_escape((*mcp_src / "server.js").string()));
    }
    const std::string preferred_server_arg = server_args.back();
    std::vector<std::string> fresh_entries;
    std::vector<std::string> injected_entries;
    fresh_entries.reserve(server_args.size());
    injected_entries.reserve(server_args.size());
    for (const auto& server_arg : server_args) {
        fresh_entries.push_back(claude_settings_fresh(server_arg));
        injected_entries.push_back(claude_settings_injected_block(server_arg));
    }

    // Case A: file was created fresh by projot — delete it
    bool is_fresh = false;
    for (const auto& fresh_entry : fresh_entries) {
        if (content == fresh_entry) {
            is_fresh = true;
            break;
        }
    }
    if (is_fresh) {
        fs::remove(settings_file, ec);
        if (ec) {
            message = "cannot remove .claude/settings.json: " + ec.message();
            return false;
        }
        message = "Removed .claude/settings.json";
        return true;
    }

    // Case B: projot injected an mcpServers block — remove the injection
    for (const auto& injected_entry : injected_entries) {
        auto pos = content.find(injected_entry);
        if (pos != std::string::npos) {
            content.erase(pos, injected_entry.size());
            std::ofstream f_write(settings_file);
            if (!f_write.is_open()) {
                message = "cannot write .claude/settings.json";
                return false;
            }
            f_write << content;
            message = "Removed projot MCP entry from .claude/settings.json";
            return true;
        }
    }

    // Case C: manually edited — give instructions
    message = "note: projot entry found in .claude/settings.json but could not be removed automatically.\n"
              "Please remove the following entry manually:\n\n"
              "    \"projot\": {\n"
              "      \"command\": \"node\",\n"
              "      \"args\": [\"" + preferred_server_arg + "\"]\n"
              "    }\n";
    return true;
}

int cmd_uninstall_mcp_server(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot uninstall-mcp-server [options]\n\n"
            "Remove the projot MCP server configuration from Claude Code and VS Code.\n\n"
            "Removes the projot entry from .claude/settings.json and deletes\n"
            ".vscode/mcp.json if it was created by projot.\n\n"
            "Optional:\n"
            "  --no-vscode    Skip VS Code (.vscode/mcp.json) removal\n\n"
            "Example:\n"
            "  projot uninstall-mcp-server\n";
        return 0;
    }

    auto root = find_repo_root();
    if (!root) {
        std::cerr << "error: not inside a git repository.\n";
        return 1;
    }

    // Remove .claude/settings.json entry
    std::string msg;
    if (!uninstall_claude_mcp(*root, msg)) {
        std::cerr << "error: " << msg << "\n";
        return 1;
    }
    std::cout << msg << "\n";

    // Remove .vscode/mcp.json unless --no-vscode
    if (!args.has("no-vscode")) {
        fs::path mcp_json = *root / ".vscode" / "mcp.json";
        std::error_code ec;
        if (!fs::exists(mcp_json, ec)) {
            std::cout << "No .vscode/mcp.json found.\n";
        } else {
            std::ifstream f(mcp_json);
            std::string content((std::istreambuf_iterator<char>(f)), {});
            f.close();
            if (content.find("\"projot\"") != std::string::npos) {
                fs::remove(mcp_json, ec);
                if (ec) {
                    std::cerr << "error: cannot remove .vscode/mcp.json: " << ec.message() << "\n";
                    return 1;
                }
                std::cout << "Removed .vscode/mcp.json\n";
            } else {
                std::cout << "projot not configured in .vscode/mcp.json.\n";
            }
        }
    }

    return 0;
}
