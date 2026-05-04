#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "config.h"

#include <fstream>
#include <filesystem>
#include <cstdio>

// Helper: write a string to a temp file and return path
static std::string write_temp(const std::string& content) {
    auto path = std::filesystem::temp_directory_path() / "projot_test_config.cfg";
    std::ofstream f(path);
    f << content;
    return path.string();
}

// ── parse_valid_full ──────────────────────────────────────────────────────────
TEST_CASE("parse_valid_full") {
    Config cfg;
    auto result = parse_config(PROJOT_TEST_DATA_DIR "/configs/valid_full.cfg", cfg);
    REQUIRE(result.ok);
    CHECK(cfg.config_version == 1);
    CHECK(cfg.app_id == "MyApp");
    CHECK(cfg.rpm == "12345");
    CHECK(cfg.name == "My Project");
    CHECK(cfg.itrack == "67890");
    REQUIRE(cfg.github.size() == 2);
    CHECK(cfg.github[0] == "https://github.com/org/repo-one");
    CHECK(cfg.github[1] == "https://github.com/org/repo-two");
    CHECK(cfg.swagger.size() == 1);
    CHECK(cfg.blizzard.size() == 1);
    REQUIRE(cfg.links.size() == 4);
    CHECK(cfg.links[0] == "teams");
    CHECK(cfg.labels["teams"] == "Teams");
    CHECK(cfg.link_urls["teams"] == "https://teams.microsoft.com/l/channel/...");
}

TEST_CASE("parse_repo_only") {
    Config cfg;
    auto result = parse_config(PROJOT_TEST_DATA_DIR "/configs/repo_only.cfg", cfg);
    REQUIRE(result.ok);
    CHECK(cfg.config_version == 1);
    CHECK(cfg.app_id == "RepoApp");
    CHECK(cfg.github.size() == 1);
    CHECK(cfg.rpm.empty());
    CHECK(cfg.name.empty());
    CHECK(cfg.itrack.empty());
    CHECK(cfg.links.empty());
}

TEST_CASE("parse_config_version_present") {
    Config cfg;
    parse_config(PROJOT_TEST_DATA_DIR "/configs/valid_full.cfg", cfg);
    CHECK(cfg.config_version == 1);
}

TEST_CASE("parse_config_version_missing") {
    Config cfg;
    parse_config(PROJOT_TEST_DATA_DIR "/configs/version_zero.cfg", cfg);
    CHECK(cfg.config_version == 0);
}

TEST_CASE("parse_config_version_future") {
    Config cfg;
    auto result = parse_config(PROJOT_TEST_DATA_DIR "/configs/version_future.cfg", cfg);
    REQUIRE(result.ok);
    CHECK(cfg.config_version == 999);
}

TEST_CASE("parse_comments_ignored") {
    auto path = write_temp("# This is a comment\napp_id = TestApp\n");
    Config cfg;
    parse_config(path, cfg);
    CHECK(cfg.app_id == "TestApp");
}

TEST_CASE("parse_blank_lines_ignored") {
    auto path = write_temp("\n\n\napp_id = BlanksApp\n\n\n");
    Config cfg;
    parse_config(path, cfg);
    CHECK(cfg.app_id == "BlanksApp");
}

TEST_CASE("parse_whitespace_trimmed") {
    auto path = write_temp("  app_id  =   TrimmedApp  \n");
    Config cfg;
    parse_config(path, cfg);
    CHECK(cfg.app_id == "TrimmedApp");
}

TEST_CASE("parse_list_field") {
    auto path = write_temp("github = https://a.com, https://b.com, https://c.com\n");
    Config cfg;
    parse_config(path, cfg);
    REQUIRE(cfg.github.size() == 3);
    CHECK(cfg.github[0] == "https://a.com");
    CHECK(cfg.github[2] == "https://c.com");
}

TEST_CASE("parse_list_single") {
    auto path = write_temp("github = https://only.com\n");
    Config cfg;
    parse_config(path, cfg);
    REQUIRE(cfg.github.size() == 1);
    CHECK(cfg.github[0] == "https://only.com");
}

TEST_CASE("parse_list_empty") {
    auto path = write_temp("github = \n");
    Config cfg;
    parse_config(path, cfg);
    CHECK(cfg.github.empty());
}

TEST_CASE("parse_link_dotted_key") {
    auto path = write_temp("link.teams = https://teams.example.com\n");
    Config cfg;
    parse_config(path, cfg);
    CHECK(cfg.link_urls["teams"] == "https://teams.example.com");
}

TEST_CASE("parse_label_dotted_key") {
    auto path = write_temp("label.teams = My Teams\n");
    Config cfg;
    parse_config(path, cfg);
    CHECK(cfg.labels["teams"] == "My Teams");
}

TEST_CASE("parse_unknown_keys_ignored") {
    auto path = write_temp("unknown_future_key = some_value\napp_id = KnownApp\n");
    Config cfg;
    auto result = parse_config(path, cfg);
    CHECK(result.ok);
    CHECK(cfg.app_id == "KnownApp");
}

TEST_CASE("parse_crlf_line_endings") {
    auto path = write_temp("app_id = CrlfApp\r\nrpm = 11111\r\n");
    Config cfg;
    parse_config(path, cfg);
    CHECK(cfg.app_id == "CrlfApp");
    CHECK(cfg.rpm == "11111");
}

TEST_CASE("parse_missing_file") {
    Config cfg;
    auto result = parse_config("/nonexistent/path/config.cfg", cfg);
    CHECK_FALSE(result.ok);
}

TEST_CASE("parse_malformed_no_equals") {
    Config cfg;
    auto result = parse_config(PROJOT_TEST_DATA_DIR "/configs/malformed.cfg", cfg);
    REQUIRE(result.ok); // malformed lines are ignored, not errors
    CHECK(cfg.app_id == "GoodApp");
    CHECK(cfg.config_version == 1);
}

TEST_CASE("write_round_trip") {
    Config orig;
    parse_config(PROJOT_TEST_DATA_DIR "/configs/valid_full.cfg", orig);

    auto out_path = (std::filesystem::temp_directory_path() / "projot_roundtrip.cfg").string();
    auto write_res = write_config(out_path, orig);
    REQUIRE(write_res.ok);

    Config reparsed;
    auto parse_res = parse_config(out_path, reparsed);
    REQUIRE(parse_res.ok);

    CHECK(reparsed.app_id == orig.app_id);
    CHECK(reparsed.rpm == orig.rpm);
    CHECK(reparsed.name == orig.name);
    CHECK(reparsed.itrack == orig.itrack);
    CHECK(reparsed.github == orig.github);
    CHECK(reparsed.swagger == orig.swagger);
    CHECK(reparsed.blizzard == orig.blizzard);
    CHECK(reparsed.links == orig.links);
    CHECK(reparsed.labels == orig.labels);
    CHECK(reparsed.link_urls == orig.link_urls);
}

TEST_CASE("write_preserves_config_version") {
    Config cfg;
    cfg.app_id = "TestApp";
    auto out_path = (std::filesystem::temp_directory_path() / "projot_version.cfg").string();
    write_config(out_path, cfg);

    Config reparsed;
    parse_config(out_path, reparsed);
    CHECK(reparsed.config_version == PROJOT_CONFIG_SCHEMA_VERSION);
}

TEST_CASE("write_comments_header") {
    Config cfg;
    cfg.app_id = "CommentApp";
    auto out_path = (std::filesystem::temp_directory_path() / "projot_comments.cfg").string();
    write_config(out_path, cfg);

    std::ifstream f(out_path);
    std::string first_line;
    std::getline(f, first_line);
    CHECK(first_line[0] == '#');
}

// ── Azure entry helpers ───────────────────────────────────────────────────────

TEST_CASE("parse_azure_entry_name_and_url") {
    auto e = parse_azure_entry("my-vault|https://portal.azure.com/vault");
    CHECK(e.name == "my-vault");
    CHECK(e.url  == "https://portal.azure.com/vault");
}

TEST_CASE("parse_azure_entry_url_only") {
    auto e = parse_azure_entry("https://portal.azure.com/dns");
    CHECK(e.name.empty());
    CHECK(e.url == "https://portal.azure.com/dns");
}

TEST_CASE("parse_azure_entry_whitespace_trimmed") {
    auto e = parse_azure_entry(" my-sub | https://portal.azure.com/sub ");
    CHECK(e.name == "my-sub");
    CHECK(e.url  == "https://portal.azure.com/sub");
}

TEST_CASE("format_azure_entry_with_name") {
    AzureEntry e{"my-cluster", "https://portal.azure.com/aks"};
    CHECK(format_azure_entry(e) == "my-cluster|https://portal.azure.com/aks");
}

TEST_CASE("format_azure_entry_url_only") {
    AzureEntry e{"", "https://portal.azure.com/dns"};
    CHECK(format_azure_entry(e) == "https://portal.azure.com/dns");
}

// ── Azure config parse/write ──────────────────────────────────────────────────

TEST_CASE("parse_azure_fields") {
    auto path = write_temp(
        "azure_subscription = sub1|https://portal.azure.com/sub1, sub2|https://portal.azure.com/sub2\n"
        "azure_key_vault = vault1|https://portal.azure.com/kv\n"
        "azure_private_dns = https://portal.azure.com/dns\n"
    );
    Config cfg;
    auto result = parse_config(path, cfg);
    REQUIRE(result.ok);
    REQUIRE(cfg.azure_subscription.size() == 2);
    CHECK(cfg.azure_subscription[0] == "sub1|https://portal.azure.com/sub1");
    CHECK(cfg.azure_subscription[1] == "sub2|https://portal.azure.com/sub2");
    REQUIRE(cfg.azure_key_vault.size() == 1);
    CHECK(cfg.azure_key_vault[0] == "vault1|https://portal.azure.com/kv");
    REQUIRE(cfg.azure_private_dns.size() == 1);
    CHECK(cfg.azure_private_dns[0] == "https://portal.azure.com/dns");
}

TEST_CASE("write_azure_round_trip") {
    Config orig;
    orig.app_id = "TestApp";
    orig.rpm   = "99999";
    orig.azure_subscription    = {"MySub|https://portal.azure.com/sub"};
    orig.azure_key_vault       = {"my-vault|https://portal.azure.com/kv"};
    orig.azure_resource_group  = {"my-rg|https://portal.azure.com/rg"};
    orig.azure_aks             = {"my-aks|https://portal.azure.com/aks"};
    orig.azure_log_analytics   = {"my-ws|https://portal.azure.com/la"};
    orig.azure_storage         = {"my-container|https://portal.azure.com/st"};
    orig.azure_private_dns     = {"https://portal.azure.com/dns"};

    auto path = (std::filesystem::temp_directory_path() / "projot_azure_rt.cfg").string();
    REQUIRE(write_config(path, orig).ok);

    Config reparsed;
    REQUIRE(parse_config(path, reparsed).ok);

    CHECK(reparsed.azure_subscription   == orig.azure_subscription);
    CHECK(reparsed.azure_key_vault      == orig.azure_key_vault);
    CHECK(reparsed.azure_resource_group == orig.azure_resource_group);
    CHECK(reparsed.azure_aks            == orig.azure_aks);
    CHECK(reparsed.azure_log_analytics  == orig.azure_log_analytics);
    CHECK(reparsed.azure_storage        == orig.azure_storage);
    CHECK(reparsed.azure_private_dns    == orig.azure_private_dns);
}

TEST_CASE("azure_absent_when_empty") {
    Config cfg;
    cfg.app_id = "TestApp";
    auto path = (std::filesystem::temp_directory_path() / "projot_no_azure.cfg").string();
    write_config(path, cfg);

    std::ifstream f(path);
    std::string content((std::istreambuf_iterator<char>(f)), {});
    CHECK(content.find("azure_") == std::string::npos);
}
