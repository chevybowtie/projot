#include "cli.h"

Args parse_args(int argc, char* argv[]) {
    Args args;
    int i = 1;

    if (i >= argc) return args;

    // Top-level --help / --version before subcommand
    {
        const std::string first = argv[i];
        if (first == "--help" || first == "-h") { args.help_requested = true; return args; }
        if (first == "--version" || first == "-v") { args.version_requested = true; return args; }
    }

    // First non-flag token is the subcommand
    if (!argv[i] || argv[i][0] != '-') {
        args.subcommand = argv[i];
        ++i;
    }

    const auto& bools = boolean_flags();

    while (i < argc) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            args.help_requested = true;
            ++i;
            continue;
        }

        if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
            std::string key = arg.substr(2);
            if (bools.count(key)) {
                // Boolean flag: no value consumed
                args.flags[key].push_back("true");
                ++i;
            } else if (i + 1 < argc) {
                args.flags[key].push_back(argv[i + 1]);
                i += 2;
            } else {
                // Flag at end of argv with no value
                args.flags[key].push_back("");
                ++i;
            }
        } else {
            // Single-dash flags → unknown; bare words → positional
            if (arg.size() >= 1 && arg[0] == '-') {
                args.unknown_flags.push_back(arg);
            } else {
                args.positional.push_back(arg);
            }
            ++i;
        }
    }

    return args;
}
