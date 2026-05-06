---
name: projot-new-command
description: Add a new subcommand to projot with boilerplate, registration, and proper helper pattern
---

# Add a New Command to projot

This skill guides you through implementing a new subcommand for projot.

## Step 1: Decide the Category

Every command belongs to one of three categories. Choose based on what the command does:

| Category | File | Examples |
|----------|------|----------|
| **Project** | `src/commands_project.cpp` | Commands that modify todos (add-todo, complete, add-note, set-link) |
| **Config** | `src/commands_config.cpp` | Commands that modify config (set-app-id, add-github, add-azure) |
| **Maintenance** | `src/commands_maint.cpp` | Utility commands (render, install-hook, install-mcp-server) |

## Step 2: Write the Command Implementation

Choose the right helper pattern:

### Pattern A: Project Modifier (modifies todos)

Use `execute_project_command()` for commands that read the project notes file, modify in-memory todos, and re-render.

```cpp
int cmd_my_project_command(const Args& args) {
    if (args.help_requested) {
        std::cout << "Usage: projot my-command [flags]\n...";
        return 0;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    return execute_project_command(ctx, [&](Project& proj) {
        // Modify proj.todos in-place
        // Return ParseResult{true, ""} on success or ParseResult{false, "error message"} on failure
        return ParseResult{true, ""};
    }, "Success message here");
}
```

### Pattern B: Config Modifier (modifies config)

Use `execute_config_command()` for commands that modify `.projot/config` and optionally re-render.

```cpp
int cmd_my_config_command(const Args& args) {
    if (args.help_requested) {
        std::cout << "Usage: projot my-config-command [flags]\n...";
        return 0;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }

    const std::string param = args.get("param");

    return execute_config_command(ctx, [&param](Context& c) {
        // Modify c.config in-place
        // Return ParseResult{true, ""} on success or ParseResult{false, "already_present"} for no-ops
        return ParseResult{true, ""};
    }, true, "Config updated");  // true = re-render project notes after config change
}
```

### Pattern C: Stateless Command (no modification)

For commands like `list` or `render` that don't modify files:

```cpp
int cmd_my_stateless_command(const Args& args) {
    if (args.help_requested) { ... }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }

    // Perform read-only operations
    // Exit with 0 (success) or 1 (error)
    return 0;
}
```

## Step 3: Declare the Function in commands.h

Add to `src/commands.h` (after the other declarations):

```cpp
int cmd_my_command(const Args& args);
```

## Step 4: Register in main.cpp

Edit `src/main.cpp`:

1. **Add to the `commands` map** (around line 75):
   ```cpp
   commands["my-command"] = cmd_my_command;
   ```

2. **Add to the `valid_flags` map** (around line 100), specifying which flags your command accepts:
   ```cpp
   valid_flags["my-command"] = {"--param", "--other-flag"};
   ```

   If your command takes a positional argument (like `add-todo "text"`), add `"<positional>"` to the list:
   ```cpp
   valid_flags["my-command"] = {"--param", "<positional>"};
   ```

## Step 5: Test

1. **Build:** `cmake --build build`
2. **Run help:** `./build/projot my-command --help`
3. **Test manually:** `./build/projot my-command [args]`
4. **Add test cases** to `tests/test_commands.cpp` if the command modifies files or has error cases.

## Common Patterns

### Argument Validation
```cpp
if (!args.has("required-flag")) {
    std::cerr << "error: --required-flag is required. Run 'projot my-command --help' for usage.\n";
    return 1;
}
const std::string value = args.get("required-flag");
```

### Handling "Already Present" (idempotent)
```cpp
return execute_config_command(ctx, [&](Context& c) {
    if (std::find(c.config.list.begin(), c.config.list.end(), value) != c.config.list.end()) {
        return ParseResult{false, "already_present"};  // Returns 0 (success), not error
    }
    c.config.list.push_back(value);
    return ParseResult{true, ""};
});
```

### Parsing Integer Arguments
```cpp
int id;
try { id = std::stoi(args.get("id")); }
catch (...) { std::cerr << "error: --id must be a number.\n"; return 1; }
```

## Do Not

- ❌ Use `std::system()` for git operations (fragile, shell injection risk)
- ❌ Use external libraries (Boost, fmt, nlohmann/json); only std::
- ❌ Use C++20 features (ranges, concepts, requires); only C++17
- ❌ Manually parse/render markdown; use `parse_markdown()` and `render_to_file()`
- ❌ Bypass `execute_project_command()` or `execute_config_command()` unless the command is read-only

## Example: Add "set-color" Config Command

```cpp
// In src/commands_config.cpp
int cmd_set_color(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot set-color --color <color>\n\n"
            "Set the project color for display.\n\n"
            "Required:\n"
            "  --color <color>   Color name (red, blue, green)\n\n"
            "Example:\n"
            "  projot set-color --color blue\n";
        return 0;
    }

    if (!args.has("color")) {
        std::cerr << "error: --color is required. Run 'projot set-color --help' for usage.\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }

    const std::string color = args.get("color");

    return execute_config_command(ctx, [&color](Context& c) {
        c.config.color = color;
        return ParseResult{true, ""};
    }, true, "Set color = " + color);
}

// In src/commands.h
int cmd_set_color(const Args& args);

// In src/main.cpp commands map
commands["set-color"] = cmd_set_color;

// In src/main.cpp valid_flags map
valid_flags["set-color"] = {"--color"};
```

Then build and test:
```bash
cmake --build build
./build/projot set-color --color blue
```
