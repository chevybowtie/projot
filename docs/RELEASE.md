# Release Process

## Overview

Final releases are created from `master` branch only. The release workflow is triggered by git tags matching `v*` (e.g., `v0.1.4`).

## Release Checklist

### 1. Prepare on feature branch

- Make all changes (code, packaging, docs) on your feature branch
- Ensure `CMakeLists.txt` has the target version number (e.g., `0.1.4`)
- Perform one final verbose test: `ctest --test-dir build --output-on-failure`

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

To publish a **pre-release** (beta, RC, etc.), append a suffix to the tag:

```sh
git tag v0.1.4-beta1
git push origin v0.1.4-beta1
```

The workflow extracts the suffix from the tag and passes it to CMake, so `projot --version` reports `0.1.4-beta1`. GitHub marks the release as a pre-release automatically when the tag contains a hyphen. The `CMakeLists.txt` version field stays as the plain numeric version (`0.1.4`); only the tag carries the suffix.

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

The numeric version is defined in `CMakeLists.txt`:

```cmake
project(projot VERSION X.Y.Z LANGUAGES CXX)
```

Bump this before creating the release tag. Pre-release suffixes (e.g. `beta1`, `rc2`) are **not** part of the CMake version — they are carried only by the git tag and appended to the binary version string automatically by the release workflow.

To test a pre-release build locally:

```sh
cmake -B build -DPROJOT_VERSION_SUFFIX=beta1
cmake --build build
./build/projot --version   # prints 0.1.4-beta1
```

## Automation notes

- Release workflow: `.github/workflows/release.yml`
- Linux packages built by: `scripts/package.sh`
- Chocolatey config: `choco/projot.nuspec` and `choco/tools/`
- CMake install rules: `CMakeLists.txt` (lines 37–49)
