# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### DOCUMENTATION

- Add summarize command to documentation and shell completions ([#e969091](https://github.com/chevybowtie/projot/commit/e96909183381a77319a552e3f85b44c602d3e077))

### FEATURES

- Add summarize command for daily todo summary ([#102e4e8](https://github.com/chevybowtie/projot/commit/102e4e8151d1521aea656670b74716ddf42245c1))

### TESTING

- Add unit tests for summarize command functionality

## [v0.1.21] - 2026-09-19

### BUG FIXES

- Quote Windows CreateProcess arguments properly; test failed carryover write ([#5d907ee](https://github.com/chevybowtie/projot/commit/5d907eeb98f63b626914c7765bd9330ef3033f71))

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

- Remove completed tech debt audit and update documentation ([#983c7ab](https://github.com/chevybowtie/projot/commit/983c7abd7407de403d5032eb763e9ca0af62a744))
- Add missing 'links' command to README documentation ([#8fb96f5](https://github.com/chevybowtie/projot/commit/8fb96f5da994f264a87a0b0ce4274c3941765eb8))
- Add Teams webhook setup instructions to README
- Drop references to removed TECH_DEBT_AUDIT.md from CLAUDE.md
- Correct CLAUDE.md gotchas on shell usage
- Add Teams workflow setup guidance
- Document todo carryover; test in-progress todos carry over

### FEATURES

- Add todo count summary to list, rename iTrack to Jira in user-facing text
- Add --jira flags as the primary spelling, keep --itrack as a synonym ([#ce0195f](https://github.com/chevybowtie/projot/commit/ce0195ff7387cf79f9901029158fa750b8327803))
- Rename itrack config keys to jira, keep reading legacy keys ([#101c1fc](https://github.com/chevybowtie/projot/commit/101c1fcf7f57e4b86e682e486a91ac42f1056257))

### BUMP

- Version 0.1.19 for release ([#c40018b](https://github.com/chevybowtie/projot/commit/c40018b3194ff89fc997affe15e85cec3a8f0584))
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

### FEATURES

- Enhance managed comment in markdown output for clarity and consistency
- Add optional teams kanban board integration
- Support pre-release suffix in versioning and update documentation

### REFACTORING

- Replace execCommand with execArgs for improved security and consistency
- Rename test case for clarity and adjust expected behavior in render command

## [v0.1.10] - 2026-05-15

### BUG FIXES

- Use bundled MCP server path in install-mcp-server config
- Resolve Windows executable path for MCP server discovery
- Expand MCP bundled path search for Windows build layout
- Convert server to JSON-RPC 2.0
- Add MCP tests

### DOCUMENTATION

- Update MCP setup docs for bundled server path flow
- Update CLAUDE.md for clarity and formatting improvements
- Update command list and installation instructions for clarity and accuracy
- Bump version for release
- Fix path

### FEATURES

- Add date prefix to notes and enhance date format handling

### REFACTORING

- Document Windows path limit in binary_dir helper
- Improve code quality for demo readiness ([#0f608d0](https://github.com/chevybowtie/projot/commit/0f608d0d5b1e6e806530fcdf6b19bd5fd187399d))

### TESTING

- Cover install/uninstall mcp config round-trip assertions

## [v0.1.9] - 2026-05-13

### BUG FIXES

- Use _dupenv_s on Windows to avoid C4996 warning ([#8e79f0b](https://github.com/chevybowtie/projot/commit/8e79f0bdd3c533e7b3298c7b0c30f1f04f5b3a10))
- Use non-const char* for _dupenv_s on Windows ([#8c2ce98](https://github.com/chevybowtie/projot/commit/8c2ce982387e3ff8db056407546629d5e932092e))
- TempGlobalConfig test helper now redirects APPDATA on Windows ([#12d60bf](https://github.com/chevybowtie/projot/commit/12d60bff2f50fff3328f7bbe6fc16dbd1ce377cb))
- Use non-const char* for _dupenv_s in TempGlobalConfig ([#6f5cd21](https://github.com/chevybowtie/projot/commit/6f5cd21166bc2e5f4216eaf5ac71ef6e3eabb5bd))

## [v0.1.8] - 2026-05-13

### BUG FIXES

- Install MCP files under usr/ so deb package places them at /usr/share/projot/mcp/

### DOCUMENTATION

- Add docstrings to all command function declarations ([#820deb1](https://github.com/chevybowtie/projot/commit/820deb1bbd42d8047cacd5bd4abec489de8d0952))
- Mark F007 and F017 as completed in tech debt audit
- Update audit with repeat-run results (2026-05-05) ([#29687b7](https://github.com/chevybowtie/projot/commit/29687b740bc6e5ccdefc0afd5321b410b6642ee3))
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

- Extract deduplicate utility to utils.h ([#a5d3556](https://github.com/chevybowtie/projot/commit/a5d3556269906f6d137f24be8b5920b93720d1ed))
- Remove legacy --text flag from add-todo and add-note ([#e945225](https://github.com/chevybowtie/projot/commit/e945225f72d1ff2b0437a6775727009fa6fde21f))
- Remove legacy ranp terminology and fix docstring ([#9cae8b9](https://github.com/chevybowtie/projot/commit/9cae8b9743ba666c3c9740f3a522040a8d2e593f))
- Merge duplicate path builder functions ([#529c433](https://github.com/chevybowtie/projot/commit/529c4336f6e4c48ac2d2fcca5c1fb0eedd20eace))
- Extract project command boilerplate pattern ([#cc67ad4](https://github.com/chevybowtie/projot/commit/cc67ad45d3b17bcda0c6b4e43c0ba56884a0d88b))
- Apply execute_project_command helper to complete and add_note ([#5fb556e](https://github.com/chevybowtie/projot/commit/5fb556e4a85f41acf98f35809cad2942ccd8ee53))
- Complete F003 boilerplate extraction and resolve F002, F004, F006, F015 ([#1915098](https://github.com/chevybowtie/projot/commit/1915098a7b454a6e6efe501d7ff8493ede961723))
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
- Setup MCP server and VSCode during `projot init` (--no-mcp to skip) (#4) ([#3f42834](https://github.com/chevybowtie/projot/commit/3f428340d4ed749a4383b63b7609a0da1e342ce8))
- Add MCP server configuration and command support

## [v0.1.4] - 2026-05-04

### BUG FIXES

- Rename ranp to rpm in write_azure_round_trip test case

### DOCUMENTATION

- Update instructions on how to use prebuilt releases
- Enhance README
- Create RELEASE standard

### FEATURES

- Add Azure resource tracking to projot projects (#5) ([#8e7db14](https://github.com/chevybowtie/projot/commit/8e7db143ce651950cb99b44d233e073877ddac20))
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
