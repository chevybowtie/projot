#include "doctest.h"
#include "user_config.h"
#include "commands.h"
#include "config.h"

#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string write_temp_user_cfg(const std::string& content,
                                       const std::string& name = "projot_test_user_config.cfg") {
    auto path = fs::temp_directory_path() / name;
    std::ofstream f(path);
    f << content;
    return path.string();
}

// ── parse_user_config ─────────────────────────────────────────────────────────

TEST_CASE("user_config: parse base_url keys") {
    auto path = write_temp_user_cfg(
        "base_url.rpm    = https://rpm.example.com/project/\n"
        "base_url.itrack = https://itrack.example.com/browse/\n"
        "base_url.github = https://github.com/\n"
        "base_url.appdb  = https://appdb.example.com/app/\n"
    );
    UserConfig cfg;
    auto result = parse_user_config(path, cfg);
    REQUIRE(result.ok);
    CHECK(cfg.base_urls["rpm"]    == "https://rpm.example.com/project/");
    CHECK(cfg.base_urls["itrack"] == "https://itrack.example.com/browse/");
    CHECK(cfg.base_urls["github"] == "https://github.com/");
    CHECK(cfg.base_urls["appdb"]  == "https://appdb.example.com/app/");
}

TEST_CASE("user_config: parse use_mcp") {
    SUBCASE("true") {
        auto path = write_temp_user_cfg("use_mcp = true\n");
        UserConfig cfg;
        parse_user_config(path, cfg);
        CHECK(cfg.use_mcp == true);
    }
    SUBCASE("false") {
        auto path = write_temp_user_cfg("use_mcp = false\n");
        UserConfig cfg;
        parse_user_config(path, cfg);
        CHECK(cfg.use_mcp == false);
    }
    SUBCASE("1") {
        auto path = write_temp_user_cfg("use_mcp = 1\n");
        UserConfig cfg;
        parse_user_config(path, cfg);
        CHECK(cfg.use_mcp == true);
    }
    SUBCASE("yes") {
        auto path = write_temp_user_cfg("use_mcp = yes\n");
        UserConfig cfg;
        parse_user_config(path, cfg);
        CHECK(cfg.use_mcp == true);
    }
}

TEST_CASE("user_config: parse install_hooks") {
    SUBCASE("true") {
        auto path = write_temp_user_cfg("install_hooks = true\n");
        UserConfig cfg;
        parse_user_config(path, cfg);
        CHECK(cfg.install_hooks == true);
    }
    SUBCASE("false") {
        auto path = write_temp_user_cfg("install_hooks = false\n");
        UserConfig cfg;
        parse_user_config(path, cfg);
        CHECK(cfg.install_hooks == false);
    }
}

TEST_CASE("user_config: defaults when file is missing") {
    UserConfig cfg;
    auto result = parse_user_config("/nonexistent/path/user_config.cfg", cfg);
    REQUIRE(result.ok);           // missing file is not an error
    CHECK(cfg.base_urls.empty());
    CHECK(cfg.use_mcp == true);
    CHECK(cfg.install_hooks == true);
}

TEST_CASE("user_config: comments and blank lines ignored") {
    auto path = write_temp_user_cfg(
        "# This is a comment\n"
        "\n"
        "use_mcp = false\n"
        "# Another comment\n"
        "\n"
    );
    UserConfig cfg;
    parse_user_config(path, cfg);
    CHECK(cfg.use_mcp == false);
    CHECK(cfg.install_hooks == true);  // default
}

TEST_CASE("user_config: whitespace trimmed") {
    auto path = write_temp_user_cfg(
        "  base_url.itrack  =   https://itrack.example.com/browse/  \n"
    );
    UserConfig cfg;
    parse_user_config(path, cfg);
    CHECK(cfg.base_urls["itrack"] == "https://itrack.example.com/browse/");
}

TEST_CASE("user_config: CRLF line endings") {
    auto path = write_temp_user_cfg("use_mcp = false\r\ninstall_hooks = false\r\n");
    UserConfig cfg;
    parse_user_config(path, cfg);
    CHECK(cfg.use_mcp == false);
    CHECK(cfg.install_hooks == false);
}

TEST_CASE("user_config: unknown keys silently ignored") {
    auto path = write_temp_user_cfg(
        "unknown_future_key = some_value\n"
        "use_mcp = false\n"
    );
    UserConfig cfg;
    auto result = parse_user_config(path, cfg);
    CHECK(result.ok);
    CHECK(cfg.use_mcp == false);
}

// ── write_user_config ─────────────────────────────────────────────────────────

TEST_CASE("user_config: write and re-parse round-trip") {
    auto path = (fs::temp_directory_path() / "projot_test_ucfg_roundtrip.cfg").string();
    std::error_code ec;
    fs::remove(path, ec);

    UserConfig cfg;
    cfg.base_urls["itrack"] = "https://itrack.example.com/browse/";
    cfg.base_urls["rpm"]    = "https://rpm.example.com/project/";
    cfg.use_mcp       = false;
    cfg.install_hooks = false;

    auto result = write_user_config(path, cfg);
    REQUIRE(result.ok);

    UserConfig parsed;
    auto parse_result = parse_user_config(path, parsed);
    REQUIRE(parse_result.ok);
    CHECK(parsed.base_urls["itrack"] == "https://itrack.example.com/browse/");
    CHECK(parsed.base_urls["rpm"]    == "https://rpm.example.com/project/");
    CHECK(parsed.use_mcp == false);
    CHECK(parsed.install_hooks == false);

    fs::remove(path, ec);
}

TEST_CASE("user_config: write creates parent directories") {
    auto dir  = fs::temp_directory_path() / "projot_test_ucfg_mkdir";
    auto path = (dir / "subdir" / "config").string();
    std::error_code ec;
    fs::remove_all(dir, ec);

    UserConfig cfg;
    auto result = write_user_config(path, cfg);
    REQUIRE(result.ok);
    CHECK(fs::exists(path));

    fs::remove_all(dir, ec);
}

// ── get_user_config_path ──────────────────────────────────────────────────────

TEST_CASE("user_config: get_user_config_path returns non-empty") {
    // HOME or APPDATA should be set in any normal test environment.
    auto path = get_user_config_path();
    CHECK_FALSE(path.empty());
}

TEST_CASE("user_config: get_user_config_path ends with projot/config") {
    auto path = get_user_config_path();
    REQUIRE_FALSE(path.empty());
    // Path must end with the canonical suffix (using / or \)
    auto p = fs::path(path);
    CHECK(p.filename() == "config");
    CHECK(p.parent_path().filename() == "projot");
}

// ── cmd_config ────────────────────────────────────────────────────────────────

// Helper: run cmd_config with a custom HOME so it doesn't touch the real user config.
struct TempHome {
    fs::path dir;
    std::string prev_home;
    std::string prev_xdg;

    explicit TempHome(const std::string& tag) {
        dir = fs::temp_directory_path() / ("projot_test_home_" + tag);
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);

        const char* h = std::getenv("HOME");
        if (h) prev_home = h;
        const char* x = std::getenv("XDG_CONFIG_HOME");
        if (x) prev_xdg = x;

#ifndef _WIN32
        setenv("HOME", dir.string().c_str(), 1);
        unsetenv("XDG_CONFIG_HOME");
#endif
    }

    ~TempHome() {
        std::error_code ec;
        fs::remove_all(dir, ec);
#ifndef _WIN32
        if (!prev_home.empty()) setenv("HOME", prev_home.c_str(), 1);
        else unsetenv("HOME");
        if (!prev_xdg.empty()) setenv("XDG_CONFIG_HOME", prev_xdg.c_str(), 1);
        else unsetenv("XDG_CONFIG_HOME");
#endif
    }

    // Path that projot should write the user config to.
    fs::path config_path() const {
        return dir / ".config" / "projot" / "config";
    }
};

TEST_CASE("cmd_config: display defaults when no file exists") {
    TempHome h("display");
    Args a;
    a.subcommand = "config";
    int rc = cmd_config(a);
    CHECK(rc == 0);
}

TEST_CASE("cmd_config: set and persist base_url_itrack") {
    TempHome h("set_itrack");
    Args a;
    a.subcommand = "config";
    a.flags["base-url-itrack"].push_back("https://itrack.example.com/browse/");
    int rc = cmd_config(a);
    REQUIRE(rc == 0);
    REQUIRE(fs::exists(h.config_path()));

    UserConfig cfg;
    parse_user_config(h.config_path().string(), cfg);
    CHECK(cfg.base_urls["itrack"] == "https://itrack.example.com/browse/");
}

TEST_CASE("cmd_config: set install_hooks false") {
    TempHome h("set_hooks");
    Args a;
    a.subcommand = "config";
    a.flags["install-hooks"].push_back("false");
    int rc = cmd_config(a);
    REQUIRE(rc == 0);

    UserConfig cfg;
    parse_user_config(h.config_path().string(), cfg);
    CHECK(cfg.install_hooks == false);
}

TEST_CASE("cmd_config: set use_mcp false") {
    TempHome h("set_mcp");
    Args a;
    a.subcommand = "config";
    a.flags["use-mcp"].push_back("false");
    int rc = cmd_config(a);
    REQUIRE(rc == 0);

    UserConfig cfg;
    parse_user_config(h.config_path().string(), cfg);
    CHECK(cfg.use_mcp == false);
}

TEST_CASE("cmd_config: set multiple values in one call") {
    TempHome h("set_multi");
    Args a;
    a.subcommand = "config";
    a.flags["base-url-rpm"].push_back("https://rpm.example.com/project/");
    a.flags["base-url-appdb"].push_back("https://appdb.example.com/app/");
    a.flags["install-hooks"].push_back("false");
    int rc = cmd_config(a);
    REQUIRE(rc == 0);

    UserConfig cfg;
    parse_user_config(h.config_path().string(), cfg);
    CHECK(cfg.base_urls["rpm"]   == "https://rpm.example.com/project/");
    CHECK(cfg.base_urls["appdb"] == "https://appdb.example.com/app/");
    CHECK(cfg.install_hooks == false);
}
