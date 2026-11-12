# Changelog

All notable changes to this project will be documented in this file.

## [v0.1.29] - 2026-11-12

### DOCUMENTATION

- Update CHANGELOG.md for 0.1.28

### FEATURES

- Experimental page sync in teams channel
- Cap Kanban Done column and show total complete (#55) ([#5a4b70a](https://github.com/chevybowtie/projot/commit/5a4b70a5ab0facb0df2b58e136bd1ecec166e001))
- Add set-project to change a project's RPM and Jira numbers (#56) ([#df95818](https://github.com/chevybowtie/projot/commit/df958185bc13fe74509cc6351a5657fa089d633a))
- Add MCP tools for set-project and the Kanban Done limit ([#1d64b9e](https://github.com/chevybowtie/projot/commit/1d64b9e704ee4947d70804774ab1178395d6bc2d))

## [v0.1.28] - 2026-10-29

### DOCUMENTATION

- Update CHANGELOG.md for 0.1.27

### FEATURES

- Post daily summary to Teams and add post toggles ([#fa6a1c9](https://github.com/chevybowtie/projot/commit/fa6a1c91283dc7482f3e0a9b4867c885ea103e88))
- Improve tab completion
- Add tests for teams post toggles and update config writing logic
- Add tests for shell completions and update project configuration
- Add unit tests for Teams post functionality
- Update project version to 0.1.28
- Refactor check_script function to avoid structured bindings for compatibility
- Normalize line endings in read_text function for cross-platform compatibility

## [v0.1.27] - 2026-10-29

### BUG FIXES

- Send default Teams webhook flow shape to Power Automate endpoints (#50) ([#6041c85](https://github.com/chevybowtie/projot/commit/6041c85dfce080eb18f41bca2a3dcc4bd8c70915))

### DOCUMENTATION

- Update CHANGELOG.md for 0.1.24
- Clarify worktree hazard instructions in CLAUDE.md
- Update CHANGELOG.md for 0.1.25-beta

### FEATURES

- Add 'add' as primary command for appending todos ([#a517b76](https://github.com/chevybowtie/projot/commit/a517b767e5f474302ca660059616216c929fc89b))

### TESTING

- Cover Power Automate Teams sync routing (#50) ([#8d29c72](https://github.com/chevybowtie/projot/commit/8d29c727a4d437d789336844ecb239c201222805))

## [v0.1.24] - 2026-09-30

### BUG FIXES

- Checkout master branch before pushing CHANGELOG.md in release workflow ([#ec7b408](https://github.com/chevybowtie/projot/commit/ec7b408176faa2bd1caab23016c557af656e4ca6))
- Add comprehensive stream state validation to file I/O operations ([#1991172](https://github.com/chevybowtie/projot/commit/19911723a01f0f788f6776dcadb4677fef1955d0))
- Add proper process creation error handling for git staging and Teams sync ([#027501a](https://github.com/chevybowtie/projot/commit/027501afdd86d832c9eaffcaa9a92e9077af3161))
- Improve robustness of config parsing, hook installation, and date formatting ([#f2f4b07](https://github.com/chevybowtie/projot/commit/f2f4b07381becc0ae96fc887ab3a235a9868bc0a))
- Enhance todo line parsing and handle unparsed lines in markdown
- Improve atomic file writing with enhanced error handling and symlink support
- Enhance file writing by rejecting invalid values and improving error handling
- Add line break validation for command-line arguments and enhance config writing checks
- Enforce strict todo section validation and improve atomic file writing
- Implement git directory and hooks directory resolution with RepoLock management
- Enhance cmd_new and config handling with improved validation and error recovery
- Improve config reading and command-line argument handling for teams-sync
- Refactor config value retrieval and enhance first list item extraction
- Enhance process handling and security guidelines in CLAUDE.md
- Add process source file and update install files for mcp
- Update DESIGN.md with detailed config item handling and pre-commit hook installation logic
- Enhance context handling with repo lock and safe RPM validation
- Add environment variable management for Git commands in tests
- Improve todo line parsing and add error handling for markdown reading
- Enhance config version validation and add tests for error handling
- Update command arguments for projot add-todo and add-note to ensure proper parsing
- Add argument rules for command execution to improve input validation
- Add sanitizers job to CI for memory error detection and undefined behavior
- Enhance argument parsing and validation for todos and notes
- Remove settings.json and update .gitignore to exclude personal Claude Code settings
- Implement timeout handling for child processes to prevent hangs and ensure proper cleanup
- Improve duplicate ID handling in todos and enhance warning messages for incomplete listings
- Enhance projot_setup_project to handle failures gracefully and rollback branch creation
- Update project version to 0.1.24

### DOCUMENTATION

- Add summarize command to documentation and shell completions ([#bfd9ca5](https://github.com/chevybowtie/projot/commit/bfd9ca50d58ac05ce1b7639239214af029327ec0))
- Update CHANGELOG.md for 0.1.22

### FEATURES

- Add summarize command for daily todo summary ([#e584344](https://github.com/chevybowtie/projot/commit/e5843444263b0ec9713f7b05426b17690a8c59fa))
- Add 'links' subcommand to project completion scripts
- Implement process management functions for cross-platform execution

### TESTING

- Add unit tests for summarize command functionality
- Enhance markdown parser and config validation with additional cases

## [v0.1.21] - 2026-09-19

### BUG FIXES

- Quote Windows CreateProcess arguments properly; test failed carryover write ([#45f85cc](https://github.com/chevybowtie/projot/commit/45f85cc77145791b985f727bdfea72fcfeb74c5d))

### DOCUMENTATION

- Refresh CLAUDE.md test count and Windows note, regenerate signatures
- Drop hard-coded test count from CLAUDE.md
- Regenerate signature index with sigmap 8.49.2
- Regenerate CLAUDE.md signature section with sigmap 8.49.2

## [v0.1.20] - 2026-09-19

### BUG FIXES

- Support Teams workflow sync endpoints
- Write carryover before archiving on close and report carried todos

### DOCUMENTATION

- Remove completed tech debt audit and update documentation ([#12fccf2](https://github.com/chevybowtie/projot/commit/12fccf2d86729d94337f146eaf56b0b7419b372f))
- Add missing 'links' command to README documentation ([#59b4ba5](https://github.com/chevybowtie/projot/commit/59b4ba59a9074cbad5c8cf52c72993eb701e6112))
- Add Teams webhook setup instructions to README
- Drop references to removed TECH_DEBT_AUDIT.md from CLAUDE.md
- Correct CLAUDE.md gotchas on shell usage
- Add Teams workflow setup guidance
- Document todo carryover; test in-progress todos carry over

### FEATURES

- Add todo count summary to list, rename iTrack to Jira in user-facing text
- Add --jira flags as the primary spelling, keep --itrack as a synonym ([#4cee0cc](https://github.com/chevybowtie/projot/commit/4cee0cc27b27708edae147ed57f06470c688b38a))
- Rename itrack config keys to jira, keep reading legacy keys ([#b01eb07](https://github.com/chevybowtie/projot/commit/b01eb07e3ce22e58085629a011beb3fd5f33fa23))

### BUMP

- Version 0.1.19 for release ([#41700ee](https://github.com/chevybowtie/projot/commit/41700ee2517bc8fea93b4a773514b65b779df230))
- Version 0.1.20

## [v0.1.17-beta] - 2026-09-10

### BUG FIXES

- Add bash-completion as a recommended package in the .deb control file

### DOCUMENTATION

- Update docs to current app state

### FEATURES

- Enhance bash completion with new subcommands and status options
- Add 'links' command to print all project URLs to the terminal

### REFACTORING

- Remove outdated Copilot and INSTRUCTIONS files, consolidate guidance in CLAUDE.md

## [v0.1.16] - 2026-06-16

### BUG FIXES

- Support MCP protocol version 2025-11-25

## [v0.1.15] - 2026-06-15

### BUG FIXES

- Declare tools capability in MCP initialize response, fix test tool names
- Add MCP protocol version negotiation in initialize handler
- Update project version to 0.1.15

## [v0.1.14] - 2026-06-15

### FEATURES

- Enable standard headers for Doctest in MSVC configuration

## [v0.1.13] - 2026-05-25

### BUG FIXES

- Reder project reports error if project is closed
- Update release process test command for consistency

### DOCUMENTATION

- Fix path

### FEATURES

- Enhance managed comment in markdown output for clarity and consistency
- Add optional teams kanban board integration
- Support pre-release suffix in versioning and update documentation

### REFACTORING

- Replace execCommand with execArgs for improved security and consistency
- Rename test case for clarity and adjust expected behavior in render command

## [v0.1.12] - 2026-05-19

### BUG FIXES

- Convert server to JSON-RPC 2.0
- Add MCP tests

### DOCUMENTATION

- Update CLAUDE.md for clarity and formatting improvements
- Update command list and installation instructions for clarity and accuracy
- Bump version for release

### REFACTORING

- Improve code quality for demo readiness ([#2c33535](https://github.com/chevybowtie/projot/commit/2c33535711df99ae94145a03afd54bbb75a94881))

## [v0.1.10] - 2026-05-15

### BUG FIXES

- Use bundled MCP server path in install-mcp-server config
- Resolve Windows executable path for MCP server discovery
- Expand MCP bundled path search for Windows build layout

### DOCUMENTATION

- Update MCP setup docs for bundled server path flow

### FEATURES

- Add date prefix to notes and enhance date format handling

### REFACTORING

- Document Windows path limit in binary_dir helper

### TESTING

- Cover install/uninstall mcp config round-trip assertions

## [v0.1.9] - 2026-05-13

### BUG FIXES

- Use _dupenv_s on Windows to avoid C4996 warning ([#ec88112](https://github.com/chevybowtie/projot/commit/ec88112f8923f017c43b3e512257affdcb015af3))
- Use non-const char* for _dupenv_s on Windows ([#56aa89e](https://github.com/chevybowtie/projot/commit/56aa89ef02d8815f221d1b5561c8dff3eba6dde5))
- TempGlobalConfig test helper now redirects APPDATA on Windows ([#4099836](https://github.com/chevybowtie/projot/commit/40998364631ca2d9d36bdb86a56f67631e92af4c))
- Use non-const char* for _dupenv_s in TempGlobalConfig ([#a96f52b](https://github.com/chevybowtie/projot/commit/a96f52b0896805bf23a58ba7ebdacae6d1119662))

## [v0.1.8] - 2026-05-13

### BUG FIXES

- Install MCP files under usr/ so deb package places them at /usr/share/projot/mcp/

### DOCUMENTATION

- Add docstrings to all command function declarations ([#4698027](https://github.com/chevybowtie/projot/commit/4698027ce2793ea74836649ad75746cca636291d))
- Mark F007 and F017 as completed in tech debt audit
- Update audit with repeat-run results (2026-05-05) ([#3757f72](https://github.com/chevybowtie/projot/commit/3757f72744817786d1d68345b42e7e2465cbb911))
- Add local .deb build instructions
- Document new features

### FEATURES

- Add guidance for implementing new commands in SKILL.md
- Add global config
- Add `close` command for complete lifecycle
- Add `close` subcommand for project archiving
- Add sigmap to this project
- Add commands for setting global defaults and closing projects

### REFACTORING

- Extract deduplicate utility to utils.h ([#0e8b420](https://github.com/chevybowtie/projot/commit/0e8b4200f452bbcf1958f7d5c52cea654df7cf36))
- Remove legacy --text flag from add-todo and add-note ([#0a560cb](https://github.com/chevybowtie/projot/commit/0a560cbc8de700d36e414e938ae67efab6b12feb))
- Remove legacy ranp terminology and fix docstring ([#9c0c35d](https://github.com/chevybowtie/projot/commit/9c0c35da2b59c437472b76b9a526c3f8fce015e1))
- Merge duplicate path builder functions ([#166ce2a](https://github.com/chevybowtie/projot/commit/166ce2a29e8dcd4acd02797b6da53e75a44acd31))
- Extract project command boilerplate pattern ([#51efdad](https://github.com/chevybowtie/projot/commit/51efdada180bc7e4928d1b3ee7cbe883c19859cd))
- Apply execute_project_command helper to complete and add_note ([#b2ed760](https://github.com/chevybowtie/projot/commit/b2ed760e4a502e1cf21ef3e3e1db1a0da05f89a3))
- Complete F003 boilerplate extraction and resolve F002, F004, F006, F015 ([#6ba355b](https://github.com/chevybowtie/projot/commit/6ba355b79e33cc3eba1a68c81b476d1f6566da91))
- Split commands.cpp by concern (F001)
- Enhance URL handling with lookup table and improve error path test coverage
- Replace std::system calls with git_stage_file helper for safer file staging
- Improve environment variable retrieval for Windows configuration

### TESTING

- Add error handling tests for cmd_render and cmd_install_hook

## [v0.1.7] - 2026-05-13

### DOCUMENTATION

- Document new features

### FEATURES

- Add global config
- Add `close` command for complete lifecycle
- Add `close` subcommand for project archiving
- Add sigmap to this project

### REFACTORING

- Improve environment variable retrieval for Windows configuration

## [v0.1.6] - 2026-05-07

### BUG FIXES

- Install MCP files under usr/ so deb package places them at /usr/share/projot/mcp/

### DOCUMENTATION

- Add local .deb build instructions

## [v0.1.5] - 2026-05-05

### BUG FIXES

- Correct markdown prefs

### DOCUMENTATION

- Cleanup docs
- Update test instructions in Developer and Release guides

### FEATURES

- Add MCP server for AI integration with projot CLI
- Setup MCP server and VSCode during `projot init` (--no-mcp to skip) (#4) ([#af9a5ff](https://github.com/chevybowtie/projot/commit/af9a5ff61ee28cde11ccfe5eed20f5a3a08c185e))
- Add MCP server configuration and command support

## [v0.1.4] - 2026-05-04

### BUG FIXES

- Rename ranp to rpm in write_azure_round_trip test case

### DOCUMENTATION

- Update instructions on how to use prebuilt releases
- Enhance README
- Create RELEASE standard

### FEATURES

- Add Azure resource tracking to projot projects (#5) ([#af49ef5](https://github.com/chevybowtie/projot/commit/af49ef5134efa644a0aa004df014f0d990a79e5a))
- Update add-note command to accept note text as a positional argument
- Add packaging support for .deb and tar.gz files, update README with installation instructions
- Add Chocolatey package support and installation instructions in README

## [v0.1.0] - 2026-05-03

### BUG FIXES

- Ignore return value of system call in cmd_render for safety
- Handle return value of system call in cmd_render for improved safety
- Ensure proper character handling in link key transformation
- Ci windows test fix for config

### DOCUMENTATION

- Add developer guide with build and installation instructions

### FEATURES

- Core modules
- CLI layer
- Shell completion
- Add CI and release workflows for automated builds and testing

<!-- generated by git-cliff -->
