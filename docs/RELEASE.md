# Release Process

## Overview

Releases are created from `master` branch only. The release workflow is triggered by git tags matching `v*` (e.g., `v0.1.4`).

## Release Checklist

### 1. Prepare on feature branch

- Make all changes (code, packaging, docs) on your feature branch
- Ensure `CMakeLists.txt` has the target version number (e.g., `0.1.4`)
- Perform one final verbose test: `./build/projot_tests`

### 2. Create PR and merge to master

```sh
# Create PR from feature branch → master
# Review, then merge to master
```

The release workflow will **not** trigger on the merge; it only triggers on tags.

### 3. Tag and release from master

Once merged to master, create and push the release tag:

```sh
git tag v0.1.4
git push origin v0.1.4
```

### 4. GitHub Actions builds and publishes

The release workflow automatically:

- Builds binaries on Linux (GCC) and Windows (MSVC)
- Runs all tests
- Creates packages:
  - `.deb` (Linux, with completions)
  - `tar.gz` (Linux, portable)
  - `.nupkg` (Windows Chocolatey package, with PowerShell completions)
- Publishes all artifacts to [GitHub Releases](../../releases)

## What gets released

| Format           | Platform | Includes                              | Install                                        |
|------------------|----------|---------------------------------------|------------------------------------------------|
| `.deb`           | Linux    | Binary + completions (bash/zsh/fish) | `sudo apt install ./projot_*.deb`              |
| `tar.gz`         | Linux    | Binary + completions + docs           | Extract and `cp` to desired location            |
| `.nupkg`         | Windows  | Binary + PowerShell completion        | `choco install projot.*.nupkg --source .`      |
| `.exe`           | Windows  | Binary only                           | Manual PATH setup                              |
| Bash/Zsh/Fish/PS1| All      | Individual completion scripts         | Manual download + installation                 |

## Version numbering

Version is defined in `CMakeLists.txt` line 2:

```cmake
project(projot VERSION X.Y.Z LANGUAGES CXX)
```

Bump this before creating the release tag.

## Automation notes

- Release workflow: `.github/workflows/release.yml`
- Linux packages built by: `scripts/package.sh`
- Chocolatey config: `choco/projot.nuspec` and `choco/tools/`
- CMake install rules: `CMakeLists.txt` (lines 37–49)
