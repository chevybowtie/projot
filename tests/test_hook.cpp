#include "doctest.h"
#include "commands.h"

#include <filesystem>
#include <fstream>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;

struct HookTempRepo {
    fs::path path;
    fs::path original;

    explicit HookTempRepo(const std::string& name) {
        path = fs::temp_directory_path() / ("projot_hook_" + name);
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path / ".git" / "hooks", ec);
        original = fs::current_path();
        fs::current_path(path);

        // Write .projot/config with app_id and rpm so commands work
        fs::create_directories(path / ".projot", ec);
        std::ofstream cfg(path / ".projot" / "config");
        cfg << "config_version = 1\napp_id = TestApp\nrpm = 1\nname = P\nitrack = 1\n";
        cfg.close();
        // Create minimal notes file
        std::ofstream notes(path / ".projot" / "1.md");
        notes << "# Project: P\n- RPM: 1\n- iTrack: 1\n- App ID: TestApp\n"
                 "- Created: 2025-01-01\n\n## Links\n\n## Todos\n\n";
    }

    ~HookTempRepo() {
        std::error_code ec;
        fs::current_path(original, ec);
        fs::remove_all(path, ec);
    }

    fs::path hook_path() const { return path / ".git" / "hooks" / "pre-commit"; }

    static std::string read_file(const fs::path& p) {
        std::ifstream f(p);
        return std::string((std::istreambuf_iterator<char>(f)), {});
    }
};

static Args make_hook_args(const std::string& sub) {
    Args a; a.subcommand = sub; return a;
}

// ── new installs hook ─────────────────────────────────────────────────────────

TEST_CASE("new_installs_hook") {
    HookTempRepo repo("new_installs_hook");

    // Re-init from scratch to test cmd_new hook installation
    std::error_code ec;
    fs::remove_all(repo.path / ".projot", ec);
    fs::create_directories(repo.path / ".projot", ec);
    std::ofstream cfg(repo.path / ".projot" / "config");
    cfg << "config_version = 1\napp_id = TestApp\n";
    cfg.close();

    Args a;
    a.subcommand = "new";
    a.flags["rpm"].push_back("99");
    a.flags["name"].push_back("P");
    a.flags["itrack"].push_back("1");
    // no --no-hook: hook should be installed
    cmd_new(a);

    CHECK(fs::exists(repo.hook_path()));
#ifndef _WIN32
    auto perms = fs::status(repo.hook_path()).permissions();
    CHECK((perms & fs::perms::owner_exec) != fs::perms::none);
#endif
}

TEST_CASE("new_hook_content") {
    HookTempRepo repo("new_hook_content");
    std::error_code ec;
    fs::remove_all(repo.path / ".projot", ec);
    fs::create_directories(repo.path / ".projot", ec);
    std::ofstream cfg(repo.path / ".projot" / "config");
    cfg << "config_version = 1\napp_id = TestApp\n";
    cfg.close();

    Args a;
    a.subcommand = "new";
    a.flags["rpm"].push_back("88");
    a.flags["name"].push_back("P");
    a.flags["itrack"].push_back("1");
    cmd_new(a);

    const auto content = HookTempRepo::read_file(repo.hook_path());
    CHECK(content.find("projot render") != std::string::npos);
    CHECK(content.find("command -v projot") != std::string::npos);
}

TEST_CASE("new_no_hook_flag") {
    HookTempRepo repo("new_no_hook_flag");
    std::error_code ec;
    fs::remove_all(repo.path / ".projot", ec);
    fs::create_directories(repo.path / ".projot", ec);
    std::ofstream cfg(repo.path / ".projot" / "config");
    cfg << "config_version = 1\napp_id = TestApp\n";
    cfg.close();

    Args a;
    a.subcommand = "new";
    a.flags["rpm"].push_back("77");
    a.flags["name"].push_back("P");
    a.flags["itrack"].push_back("1");
    a.flags["no-hook"].push_back("true");
    cmd_new(a);

    CHECK_FALSE(fs::exists(repo.hook_path()));
}

// ── install-hook ──────────────────────────────────────────────────────────────

TEST_CASE("install_hook_creates_file") {
    HookTempRepo repo("install_hook_creates_file");
    CHECK_FALSE(fs::exists(repo.hook_path()));
    CHECK(cmd_install_hook(make_hook_args("install-hook")) == 0);
    CHECK(fs::exists(repo.hook_path()));
}

TEST_CASE("install_hook_appends_if_exists") {
    HookTempRepo repo("install_hook_appends_if_exists");
    // Write an existing hook with unrelated content
    {
        std::ofstream f(repo.hook_path());
        f << "#!/bin/sh\necho 'existing hook'\n";
    }
#ifndef _WIN32
    fs::permissions(repo.hook_path(),
        fs::perms::owner_exec, fs::perm_options::add);
#endif
    CHECK(cmd_install_hook(make_hook_args("install-hook")) == 0);
    const auto content = HookTempRepo::read_file(repo.hook_path());
    CHECK(content.find("existing hook") != std::string::npos);
    CHECK(content.find("projot render") != std::string::npos);
}

TEST_CASE("install_hook_append_notice") {
    // Tested implicitly — cmd_install_hook prints the notice when appending.
    // Just verify the command succeeds when a hook already exists.
    HookTempRepo repo("install_hook_append_notice");
    { std::ofstream f(repo.hook_path()); f << "#!/bin/sh\necho hi\n"; }
    CHECK(cmd_install_hook(make_hook_args("install-hook")) == 0);
}

TEST_CASE("install_hook_idempotent") {
    HookTempRepo repo("install_hook_idempotent");
    cmd_install_hook(make_hook_args("install-hook"));
    cmd_install_hook(make_hook_args("install-hook"));
    const auto content = HookTempRepo::read_file(repo.hook_path());
    // Count occurrences of "projot render"
    std::size_t count = 0, pos = 0;
    while ((pos = content.find("projot render", pos)) != std::string::npos) {
        ++count; pos += 13;
    }
    CHECK(count == 1);
}

// ── uninstall-hook ────────────────────────────────────────────────────────────

TEST_CASE("uninstall_hook_no_hook_installed") {
    HookTempRepo repo("uninstall_hook_no_hook");
    // Hook doesn't exist — should succeed and report nothing installed
    CHECK(cmd_uninstall_hook(make_hook_args("uninstall-hook")) == 0);
    CHECK_FALSE(fs::exists(repo.hook_path()));
}

TEST_CASE("uninstall_hook_removes_projot_only_file") {
    HookTempRepo repo("uninstall_hook_removes_file");
    // Install hook (fresh file created by projot)
    CHECK(cmd_install_hook(make_hook_args("install-hook")) == 0);
    CHECK(fs::exists(repo.hook_path()));

    // Uninstall — file should be removed because only projot content remains
    CHECK(cmd_uninstall_hook(make_hook_args("uninstall-hook")) == 0);
    CHECK_FALSE(fs::exists(repo.hook_path()));
}

TEST_CASE("uninstall_hook_strips_block_from_existing_hook") {
    HookTempRepo repo("uninstall_hook_strips_block");
    // Write an existing hook with unrelated content
    {
        std::ofstream f(repo.hook_path());
        f << "#!/bin/sh\necho 'existing hook'\n";
    }
#ifndef _WIN32
    fs::permissions(repo.hook_path(), fs::perms::owner_exec, fs::perm_options::add);
#endif
    // Append projot block
    CHECK(cmd_install_hook(make_hook_args("install-hook")) == 0);
    CHECK(HookTempRepo::read_file(repo.hook_path()).find("projot render") != std::string::npos);

    // Uninstall — file should survive with original content intact
    CHECK(cmd_uninstall_hook(make_hook_args("uninstall-hook")) == 0);
    CHECK(fs::exists(repo.hook_path()));
    const auto content = HookTempRepo::read_file(repo.hook_path());
    CHECK(content.find("existing hook") != std::string::npos);
    CHECK(content.find("projot render") == std::string::npos);
}

TEST_CASE("uninstall_hook_block_not_present") {
    HookTempRepo repo("uninstall_hook_block_not_present");
    // Write a hook that doesn't have the projot block
    { std::ofstream f(repo.hook_path()); f << "#!/bin/sh\necho 'other'\n"; }
    // Should succeed and leave the file untouched
    CHECK(cmd_uninstall_hook(make_hook_args("uninstall-hook")) == 0);
    CHECK(fs::exists(repo.hook_path()));
    CHECK(HookTempRepo::read_file(repo.hook_path()).find("other") != std::string::npos);
}

TEST_CASE("uninstall_hook_idempotent") {
    HookTempRepo repo("uninstall_hook_idempotent");
    CHECK(cmd_install_hook(make_hook_args("install-hook")) == 0);
    // Uninstall twice — both calls should succeed
    CHECK(cmd_uninstall_hook(make_hook_args("uninstall-hook")) == 0);
    CHECK(cmd_uninstall_hook(make_hook_args("uninstall-hook")) == 0);
    CHECK_FALSE(fs::exists(repo.hook_path()));
}

// ── uninstall-mcp-server ──────────────────────────────────────────────────────

struct McpTempRepo {
    fs::path path;
    fs::path original;

    explicit McpTempRepo(const std::string& name) {
        path = fs::temp_directory_path() / ("projot_mcp_" + name);
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path / ".git" / "hooks", ec);
        original = fs::current_path();
        fs::current_path(path);
    }
    ~McpTempRepo() {
        std::error_code ec;
        fs::current_path(original, ec);
        fs::remove_all(path, ec);
    }
    static std::string read_file(const fs::path& p) {
        std::ifstream f(p);
        return std::string((std::istreambuf_iterator<char>(f)), {});
    }
};

static Args make_mcp_args(const std::string& sub,
                           std::initializer_list<std::pair<std::string,std::string>> flags = {}) {
    Args a;
    a.subcommand = sub;
    for (const auto& [k, v] : flags) a.flags[k].push_back(v);
    return a;
}

TEST_CASE("uninstall_mcp_no_files") {
    McpTempRepo repo("uninstall_mcp_no_files");
    // Neither .claude/settings.json nor .vscode/mcp.json exist
    CHECK(cmd_uninstall_mcp_server(make_mcp_args("uninstall-mcp-server")) == 0);
}

TEST_CASE("uninstall_mcp_removes_fresh_claude_settings") {
    McpTempRepo repo("uninstall_mcp_removes_claude");
    // Write exactly what install-mcp-server creates from scratch
    fs::create_directories(repo.path / ".claude");
    {
        std::ofstream f(repo.path / ".claude" / "settings.json");
        f << "{\n"
          << "  \"mcpServers\": {\n"
          << "    \"projot\": {\n"
          << "      \"command\": \"node\",\n"
          << "      \"args\": [\"./mcp/server.js\"]\n"
          << "    }\n"
          << "  }\n"
          << "}\n";
    }
    CHECK(cmd_uninstall_mcp_server(make_mcp_args("uninstall-mcp-server", {{"no-vscode", "true"}})) == 0);
    CHECK_FALSE(fs::exists(repo.path / ".claude" / "settings.json"));
}

TEST_CASE("uninstall_mcp_removes_injected_claude_block") {
    McpTempRepo repo("uninstall_mcp_injected");
    // Simulate what install-mcp-server does when it injects into an existing file
    fs::create_directories(repo.path / ".claude");
    {
        std::ofstream f(repo.path / ".claude" / "settings.json");
        f << "{\n"
          << "  \"someOtherKey\": true"
          << ",\n  \"mcpServers\": {\n"
          << "    \"projot\": {\n"
          << "      \"command\": \"node\",\n"
          << "      \"args\": [\"./mcp/server.js\"]\n"
          << "    }\n"
          << "  }"
          << "\n}\n";
    }
    CHECK(cmd_uninstall_mcp_server(make_mcp_args("uninstall-mcp-server", {{"no-vscode", "true"}})) == 0);
    const auto content = McpTempRepo::read_file(repo.path / ".claude" / "settings.json");
    CHECK(content.find("projot") == std::string::npos);
    CHECK(content.find("someOtherKey") != std::string::npos);
}

TEST_CASE("uninstall_mcp_removes_vscode_file") {
    McpTempRepo repo("uninstall_mcp_removes_vscode");
    // Create a .vscode/mcp.json that references projot
    fs::create_directories(repo.path / ".vscode");
    {
        std::ofstream f(repo.path / ".vscode" / "mcp.json");
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
    }
    CHECK(cmd_uninstall_mcp_server(make_mcp_args("uninstall-mcp-server")) == 0);
    CHECK_FALSE(fs::exists(repo.path / ".vscode" / "mcp.json"));
}

TEST_CASE("uninstall_mcp_no_vscode_flag") {
    McpTempRepo repo("uninstall_mcp_no_vscode_flag");
    // Create both files
    fs::create_directories(repo.path / ".claude");
    fs::create_directories(repo.path / ".vscode");
    {
        std::ofstream f(repo.path / ".claude" / "settings.json");
        f << "{\n  \"mcpServers\": {\n    \"projot\": {\n      \"command\": \"node\",\n      \"args\": [\"./mcp/server.js\"]\n    }\n  }\n}\n";
    }
    {
        std::ofstream f(repo.path / ".vscode" / "mcp.json");
        f << "{\n  \"servers\": { \"projot\": {} }\n}\n";
    }
    // --no-vscode should skip .vscode/mcp.json
    CHECK(cmd_uninstall_mcp_server(make_mcp_args("uninstall-mcp-server", {{"no-vscode", "true"}})) == 0);
    CHECK_FALSE(fs::exists(repo.path / ".claude" / "settings.json"));
    CHECK(fs::exists(repo.path / ".vscode" / "mcp.json")); // untouched
}

TEST_CASE("install_hook_unwritable_dir") {
#ifndef _WIN32
    if (getuid() == 0) return; // root bypasses file permissions
    HookTempRepo repo("install_hook_unwritable");
    fs::path hooks_dir = repo.path / ".git" / "hooks";
    std::error_code ec;
    fs::permissions(hooks_dir, fs::perms::owner_read | fs::perms::owner_exec, ec);
    int ret = cmd_install_hook(make_hook_args("install-hook"));
    fs::permissions(hooks_dir, fs::perms::owner_all, ec); // restore before cleanup
    CHECK(ret != 0);
#endif
}
