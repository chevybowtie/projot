#include "cli.h"
#include "config.h"  // trim()
#include "utils.h"

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

        // "--" ends option parsing, so free text that starts with '-' (e.g. a todo
        // "--verbose flag is broken") can still be passed as a positional argument.
        if (arg == "--") {
            for (++i; i < argc; ++i) args.positional.push_back(argv[i]);
            break;
        }

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

std::string empty_flag_value_error(const Args& args) {
    const auto& bools = boolean_flags();
    for (const auto& [key, values] : args.flags) {
        if (bools.count(key)) continue;
        for (const auto& value : values)
            if (trim(value).empty()) return "--" + key + " requires a value.";
    }
    return "";
}

std::string line_break_arg_error(const Args& args) {
    for (const auto& [key, values] : args.flags)
        for (const auto& value : values)
            if (has_line_break(value)) return "--" + key + " must not contain line breaks.";
    for (const auto& value : args.positional)
        if (has_line_break(value)) return "arguments must not contain line breaks.";
    return "";
}

void normalize_flag_aliases(Args& args) {
    static const std::map<std::string, std::string> aliases{
        {"itrack",          "jira"},
        {"itrack-url",      "jira-url"},
        {"itrack-base-url", "jira-base-url"},
    };
    for (const auto& [alias, canonical] : aliases) {
        auto it = args.flags.find(alias);
        if (it == args.flags.end()) continue;
        if (args.flags.find(canonical) == args.flags.end())
            args.flags[canonical] = it->second;
        args.flags.erase(alias);
    }
}
