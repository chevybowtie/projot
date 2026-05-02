#include "cli.h"
#include "commands.h"

#include <iostream>
#include <map>
#include <set>
#include <functional>

#ifndef PROJOT_VERSION
#define PROJOT_VERSION "0.0.0"
#endif

static void print_usage() {
    std::cout <<
        "Usage: projot <subcommand> [options]\n\n"
        "Setup commands:\n"
        "  init          Initialize projot for this repository\n"
        "  new           Start a new RANP project in this repository\n\n"
        "Project commands:\n"
        "  add-todo      Append a new todo\n"
        "  list          Show project summary and todos\n"
        "  complete      Mark a todo completed\n"
        "  add-note      Add a note to a todo\n"
        "  set-link      Set or update a single-value link URL\n"
        "  set-app-id    Set the application ID\n"
        "  add-github    Add a GitHub URL\n"
        "  add-swagger   Add a Swagger URL\n"
        "  add-blizzard  Add a Blizzard URL\n"
        "  render        Re-render the notes file and stage it\n\n"
        "Maintenance commands:\n"
        "  install-hook  Install the pre-commit git hook\n\n"
        "Run 'projot <subcommand> --help' for subcommand options.\n";
}

// Valid flags per subcommand (used to detect unknown flags).
static const std::map<std::string, std::set<std::string>>& valid_flags() {
    static const std::map<std::string, std::set<std::string>> m{
        {"init",         {"app-id", "github", "swagger", "blizzard"}},
        {"new",          {"ranp", "name", "itrack", "teams", "ranp-url",
                          "itrack-url", "other", "no-hook"}},
        {"add-todo",     {"text"}},
        {"list",         {"open", "closed", "all"}},
        {"complete",     {"todo"}},
        {"add-note",     {"todo", "text"}},
        {"set-link",     {"key", "url"}},
        {"set-app-id",   {"app-id", "force"}},
        {"add-github",   {"url"}},
        {"add-swagger",  {"url"}},
        {"add-blizzard", {"url"}},
        {"render",       {}},
        {"install-hook", {}},
    };
    return m;
}

int main(int argc, char* argv[]) {
    Args args = parse_args(argc, argv);

    if (args.version_requested) {
        std::cout << "projot " << PROJOT_VERSION << "\n";
        return 0;
    }

    if (args.subcommand.empty()) {
        if (args.help_requested) { print_usage(); return 0; }
        std::cerr << "error: no subcommand specified. Run 'projot --help' for usage.\n";
        return 1;
    }

    using CmdFn = std::function<int(const Args&)>;
    static const std::map<std::string, CmdFn> commands{
        {"init",         cmd_init},
        {"new",          cmd_new},
        {"add-todo",     cmd_add_todo},
        {"list",         cmd_list},
        {"complete",     cmd_complete},
        {"add-note",     cmd_add_note},
        {"set-link",     cmd_set_link},
        {"set-app-id",   cmd_set_app_id},
        {"add-github",   cmd_add_github},
        {"add-swagger",  cmd_add_swagger},
        {"add-blizzard", cmd_add_blizzard},
        {"render",       cmd_render},
        {"install-hook", cmd_install_hook},
    };

    auto cmd_it = commands.find(args.subcommand);
    if (cmd_it == commands.end()) {
        std::cerr << "error: unknown subcommand '" << args.subcommand
                  << "'. Run 'projot --help' for usage.\n";
        return 1;
    }

    // Check for unknown flags (skip when --help is requested — let the command handle it)
    if (!args.help_requested) {
        auto vf_it = valid_flags().find(args.subcommand);
        if (vf_it != valid_flags().end()) {
            for (const auto& [flag, _] : args.flags) {
                if (!vf_it->second.count(flag)) {
                    std::cerr << "error: unknown flag '--" << flag << "'. "
                              << "Run 'projot " << args.subcommand
                              << " --help' for usage.\n";
                    return 1;
                }
            }
        }
        // Reject stray non-flag tokens (e.g. single-dash flags)
        if (!args.unknown_flags.empty()) {
            std::cerr << "error: unknown flag '" << args.unknown_flags[0] << "'. "
                      << "Run 'projot " << args.subcommand << " --help' for usage.\n";
            return 1;
        }
    }

    return cmd_it->second(args);
}
