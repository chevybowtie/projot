# Developer Guide

## Requirements

| Tool | Minimum version |
|------|----------------|
| C++ compiler | GCC 9+, Clang 10+, or MSVC 2019+ (C++17 required) |
| CMake | 3.16+ |

No other dependencies are needed. The test framework ([doctest](https://github.com/doctest/doctest)) is vendored in `tests/doctest.h`.

---

## Building

A `Makefile` wrapper is provided for Linux convenience. On Windows, use the CMake commands directly.

| Target | Action |
|---|---|
| `make` | Configure (Release) and build |
| `make debug` | Configure (Debug) and build |
| `make test` | Build (Debug) and run all tests |
| `make install` | Install binary to `$(PREFIX)/bin` (default: `/usr/local`) |
| `make install-completion` | Install shell completion for the current shell |
| `make clean` | Remove the `build/` directory |

### Build with make (Linux)

```sh
make          # release build
make debug    # debug build
```

### Configure and build directly with CMake (all platforms)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The compiled binary is at `build/projot` (or `build\projot.exe` on Windows).

---

## Running the tests

Run the test binary for verbose output:

```sh
./build/projot_tests
./build/projot_tests --test-case="render_header"   # run a single case
./build/projot_tests --list-test-cases              # list all cases
```

Or use ctest for a summary:

```sh
ctest --test-dir build --output-on-failure
```

All tests are compiled into a single `projot_tests` binary and registered as one CTest entry.

---

## Installing

### System-wide (Linux/macOS)

```sh
make
sudo make install          # installs to /usr/local/bin
```

Or with CMake directly:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

### Per-user (Linux/macOS)

```sh
make install PREFIX=~/.local
```

Make sure `~/.local/bin` is in your `PATH`.

### Windows

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_INSTALL_PREFIX="C:\Program Files\projot"
cmake --build build --config Release
cmake --install build --config Release
```

Add the install directory to your system `PATH` via **System Properties → Environment Variables**.

---

## Shell completion

Completion scripts for Bash, Zsh, Fish, and PowerShell live in `completions/`.

Install for the current shell automatically:

```sh
make install-completion
```

| Shell | Installed path |
|---|---|
| Bash | `~/.local/share/bash-completion/completions/projot` |
| Zsh | `~/.zsh/completions/_projot` (must be on `$fpath`) |
| Fish | `~/.config/fish/completions/projot.fish` |
| PowerShell | Manual — dot-source `completions/projot.ps1` from `$PROFILE` |

Restart your shell after installing, or source the file directly.

---

## Project layout

```
src/          Core library + main entry point
tests/        doctest test files and fixture data
completions/  Shell completion scripts (bash, zsh, fish, powershell)
scripts/      Developer helper scripts
docs/         Design and developer documentation
build/        CMake build output (not committed)
```

---

## Compile-time definitions

These are set by CMake and baked into the binary:

| Definition | Value | Purpose |
|---|---|---|
| `PROJOT_VERSION` | `"0.1.0"` | Reported by `projot --version` |
| `PROJOT_CONFIG_VERSION` | `1` | Current `.projot/config` schema version |
