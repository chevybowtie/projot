# Automated Release Notes with git-cliff

This project uses **git-cliff** to automatically generate release notes from conventional commits.

## How It Works

1. **Commits are tagged** with conventional commit types:
   - `feat:` → Features
   - `fix:` → Bug Fixes
   - `doc:` → Documentation
   - `perf:` → Performance
   - `refactor:` → Refactoring
   - `test:` → Testing
   - `chore:` and `ci:` → (skipped)

2. **Release workflow** (triggered by `git tag v*`):
   - Builds binaries and packages
   - Runs `git-cliff` to parse commits since last tag
   - Generates `CHANGELOG.md` and extracts release notes
   - Publishes to GitHub Releases with formatted notes

3. **Configuration** is in [cliff.toml](../cliff.toml)

## Local Testing

To test changelog generation locally:

```bash
# Install git-cliff (requires Rust)
cargo install git-cliff

# Generate the full changelog
scripts/generate-changelog.sh

# View the generated CHANGELOG.md
cat CHANGELOG.md
```

## Release Process

See [RELEASE.md](RELEASE.md) for the full release checklist (branch prep, tagging, pre-release suffixes). Once a `v*` tag is pushed, GitHub Actions builds all platforms, generates the changelog, and creates the GitHub Release with formatted notes.

## Example Release Notes

Generated notes are structured by commit type:

```text
## [0.1.8] - 2026-05-13

### Features
- add MCP server configuration and command support
- setup MCP server and VSCode during `projot init`

### Bug Fixes
- install MCP files under usr/ so deb package places them correctly

### Documentation
- document new features
```

## Customization

Edit `cliff.toml` to:

- Add new commit types
- Change the release notes format
- Modify groupings or filtering

See [git-cliff docs](https://github.com/orhun/git-cliff) for more options.
