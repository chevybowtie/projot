#include "commands.h"
#include "commands_internal.h"
#include "config.h"
#include "markdown.h"
#include "todo.h"
#include "utils.h"

#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// ── close ─────────────────────────────────────────────────────────────────────

int cmd_close(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot close\n\n"
            "Archive the current project and reset for the next one.\n\n"
            "Moves the project notes file to .projot/archive/ and clears all\n"
            "project-level configuration (rpm, itrack, todos, etc).\n"
            "Repo-level settings (app_id, GitHub, Swagger, Blizzard) are preserved.\n\n"
            "No flags required.\n\n"
            "Example:\n"
            "  projot close\n";
        return 0;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    fs::path archive_dir = ctx.repo_root / ".projot" / "archive";
    std::error_code ec;
    fs::create_directories(archive_dir, ec);
    if (ec) {
        std::cerr << "error: cannot create archive directory: " << ec.message() << "\n";
        return 1;
    }

    fs::path old_notes = ctx.repo_root / ".projot" / (ctx.config.rpm + ".md");
    fs::path archived_notes = archive_dir / (ctx.config.rpm + ".md");
    fs::rename(old_notes, archived_notes, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        std::cerr << "error: cannot archive notes file: " << ec.message() << "\n";
        return 1;
    }

    std::cout << "Archived project: " << ctx.config.name
              << "  |  RPM: " << ctx.config.rpm
              << "  |  Created: " << ctx.config.created << "\n";

    ctx.config.rpm = "";
    ctx.config.name = "";
    ctx.config.itrack = "";
    ctx.config.created = "";
    ctx.config.date_format = "";
    ctx.config.links.clear();
    ctx.config.labels.clear();
    ctx.config.link_urls.clear();
    ctx.config.azure_subscription.clear();
    ctx.config.azure_key_vault.clear();
    ctx.config.azure_resource_group.clear();
    ctx.config.azure_aks.clear();
    ctx.config.azure_log_analytics.clear();
    ctx.config.azure_storage.clear();
    ctx.config.azure_private_dns.clear();

    auto save = write_config(projot_file_path(ctx, "config"), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    std::cout << "Closed. Run 'projot new' to start the next project.\n";
    return 0;
}

// ── add-todo ──────────────────────────────────────────────────────────────────

int cmd_add_todo(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot add-todo \"<description>\"\n\n"
            "Append a new todo to the project notes file.\n\n"
            "Required:\n"
            "  \"<description>\"   Text of the new todo\n\n"
            "Example:\n"
            "  projot add-todo \"Validate index rebuild plan\"\n";
        return 0;
    }

    if (args.positional.empty()) {
        std::cerr << "error: todo text is required. Run 'projot add-todo --help' for usage.\n";
        return 1;
    }
    std::string text = args.positional[0];

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    return execute_project_command(ctx, [&](Project& proj) {
        Todo t;
        t.id           = next_todo_id(proj.todos);
        t.text         = text;
        t.created_date = date_today();
        proj.todos.push_back(t);
        std::cout << "Added todo " << t.id << ": " << t.text << "\n";
        return ParseResult{true, ""};
    });
}

// ── list ─────────────────────────────────────────────────────────────────────

int cmd_list(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot list [--open | --closed | --all]\n\n"
            "Display project summary and todos.\n\n"
            "Optional:\n"
            "  --open    Show open todos only (default)\n"
            "  --closed  Show closed todos only\n"
            "  --all     Show all todos\n\n"
            "Example:\n"
            "  projot list\n"
            "  projot list --all\n";
        return 0;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    TodoFilter filter = TodoFilter::Open;
    if (args.has("closed")) filter = TodoFilter::Closed;
    else if (args.has("all")) filter = TodoFilter::All;

    std::cout << "Project: " << ctx.config.name
              << "  |  RPM: " << ctx.config.rpm
              << "  |  iTrack: " << (ctx.config.itrack.empty() ? "N/A" : ctx.config.itrack)
              << "\n\n";

    auto todos = filter_todos(proj.todos, filter);
    if (todos.empty()) {
        std::cout << "(no todos)\n";
    } else {
        for (const auto* t : todos) {
            std::cout << t->id << ". "
                      << (t->completed ? "[x]" : "[ ]")
                      << " " << t->text << "\n";
        }
    }
    return 0;
}

// ── complete ─────────────────────────────────────────────────────────────────

int cmd_complete(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot complete --todo <ID>\n\n"
            "Mark a todo as completed.\n\n"
            "Required:\n"
            "  --todo <ID>   Stable numeric todo ID\n\n"
            "Example:\n"
            "  projot complete --todo 1\n";
        return 0;
    }

    if (!args.has("todo")) {
        std::cerr << "error: --todo is required. Run 'projot complete --help' for usage.\n";
        return 1;
    }

    int id;
    try { id = std::stoi(args.get("todo")); }
    catch (...) { std::cerr << "error: --todo must be a number.\n"; return 1; }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    return execute_project_command(ctx, [id](Project& proj) {
        if (!find_todo(proj.todos, id)) {
            std::string msg = "todo " + std::to_string(id) + " not found.";
            if (!proj.todos.empty()) {
                msg += " Valid IDs:";
                for (const auto& t : proj.todos) msg += " " + std::to_string(t.id);
            }
            return ParseResult{false, msg};
        }

        auto result = complete_todo(proj.todos, id, date_today());
        if (result.warned) {
            std::cerr << "warning: " << result.message << "\n";
            // Don't modify the project if warning occurred
            return ParseResult{false, "already_completed"};
        }

        std::cout << "Completed todo " << id << ".\n";
        return ParseResult{true, ""};
    });
}

// ── add-note ─────────────────────────────────────────────────────────────────

int cmd_add_note(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot add-note --todo <ID> \"<note>\"\n\n"
            "Add a note to a todo.\n\n"
            "Required:\n"
            "  --todo <ID>    Stable numeric todo ID\n"
            "  \"<note>\"       Note text\n\n"
            "Example:\n"
            "  projot add-note --todo 1 \"Waiting on supervisor feedback\"\n";
        return 0;
    }

    if (!args.has("todo")) {
        std::cerr << "error: --todo is required. "
                     "Run 'projot add-note --help' for usage.\n";
        return 1;
    }

    if (args.positional.empty()) {
        std::cerr << "error: note text is required. Run 'projot add-note --help' for usage.\n";
        return 1;
    }

    int id;
    try { id = std::stoi(args.get("todo")); }
    catch (...) { std::cerr << "error: --todo must be a number.\n"; return 1; }

    std::string text = args.positional[0];

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    return execute_project_command(ctx, [id, &text](Project& proj) {
        if (!find_todo(proj.todos, id)) {
            std::string msg = "todo " + std::to_string(id) + " not found.";
            if (!proj.todos.empty()) {
                msg += " Valid IDs:";
                for (const auto& t : proj.todos) msg += " " + std::to_string(t.id);
            }
            return ParseResult{false, msg};
        }

        auto result = add_note(proj.todos, id, text);
        if (result.warned) std::cerr << "warning: " << result.message << "\n";

        std::cout << "Added note to todo " << id << ".\n";
        return ParseResult{true, ""};
    });
}

// ── set-link ─────────────────────────────────────────────────────────────────

int cmd_set_link(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot set-link --key <key> --url <URL>\n\n"
            "Set or update a single-value link URL.\n\n"
            "Required:\n"
            "  --key <key>   Link key (e.g. teams, itrack, rpm, other)\n"
            "  --url <URL>   URL value\n\n"
            "Example:\n"
            "  projot set-link --key teams --url https://teams.microsoft.com/...\n";
        return 0;
    }

    if (!args.has("key") || !args.has("url")) {
        std::cerr << "error: --key and --url are required. "
                     "Run 'projot set-link --help' for usage.\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    const std::string key = args.get("key");
    const std::string url = args.get("url");

    return execute_config_command(ctx, [key, &url](Context& c) {
        c.config.link_urls[key] = url;
        if (std::find(c.config.links.begin(), c.config.links.end(), key)
                == c.config.links.end()) {
            c.config.links.push_back(key);
        }
        return ParseResult{true, ""};
    }, true, "Set link " + key + " = " + url);
}
