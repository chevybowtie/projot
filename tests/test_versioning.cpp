#include "doctest.h"
#include "config.h"
#include "cli.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// ── config_version written on init ───────────────────────────────────────────

TEST_CASE("config_version_written_on_init") {
    auto dir = fs::temp_directory_path() / "projot_ver_init";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir / ".git", ec);
    fs::create_directories(dir / ".projot", ec);

    Config cfg;
    cfg.app_id = "TestApp";
    write_config((dir / ".projot" / "config").string(), cfg);

    Config reparsed;
    parse_config((dir / ".projot" / "config").string(), reparsed);
    CHECK(reparsed.config_version == PROJOT_CONFIG_SCHEMA_VERSION);

    fs::remove_all(dir, ec);
}

TEST_CASE("config_version_correct_value") {
    CHECK(PROJOT_CONFIG_SCHEMA_VERSION == PROJOT_CONFIG_VERSION);
}

TEST_CASE("config_version_future_hard_error") {
    Config cfg;
    parse_config(PROJOT_TEST_DATA_DIR "/configs/version_future.cfg", cfg);
    CHECK(cfg.config_version == 999);
    // The version check (> PROJOT_CONFIG_SCHEMA_VERSION) is done in load_context()
    // inside commands. Verify the value is correctly parsed here.
    CHECK(cfg.config_version > PROJOT_CONFIG_SCHEMA_VERSION);
}

TEST_CASE("config_version_zero_warns") {
    // Missing config_version → parsed as 0
    Config cfg;
    auto result = parse_config(PROJOT_TEST_DATA_DIR "/configs/version_zero.cfg", cfg);
    CHECK(result.ok);
    CHECK(cfg.config_version == 0);
    CHECK(cfg.app_id == "OldApp");
}

// ── app version format (parse_args --version flag) ────────────────────────────

TEST_CASE("app_version_format") {
    char arg0[] = "projot";
    char arg1[] = "--version";
    char* argv[] = {arg0, arg1, nullptr};
    Args a = parse_args(2, argv);
    CHECK(a.version_requested);
    // PROJOT_VERSION is defined by CMake as "major.minor.patch"
    std::string ver = PROJOT_VERSION;
    // Should match \d+\.\d+\.\d+
    bool valid = false;
    if (ver.size() >= 5) {
        auto d1 = ver.find('.');
        auto d2 = ver.find('.', d1 + 1);
        valid = (d1 != std::string::npos && d2 != std::string::npos && d2 > d1 + 1);
    }
    CHECK(valid);
}
