# Developer Guide

## Requirements

| Tool | Minimum version |
|------|----------------|
| C++ compiler | GCC 9+, Clang 10+, or MSVC 2019+ (C++17 required) |
| CMake | 3.16+ |

No other dependencies are needed. The test framework ([doctest](https://github.com/doctest/doctest)) is vendored in `tests/doctest.h`.

---

## Building

### Configure and build (debug)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Configure and build (release)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The compiled binary is at `build/projot` (or `build\projot.exe` on Windows).

---

## Running the tests

```sh
ctest --test-dir build --output-on-failure
```

All tests are compiled into a single `projot_tests` binary and registered as one CTest entry. The `--output-on-failure` flag prints the doctest output only for failing cases.

To run the test binary directly for more verbose output:

```sh
./build/projot_tests
./build/projot_tests --test-case="render_header"   # run a single case
./build/projot_tests --list-test-cases              # list all cases
```

---

## Installing

### System-wide (Linux/macOS)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

This copies the `projot` binary to `/usr/local/bin`.

### Per-user (Linux/macOS)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

Make sure `$HOME/.local/bin` is in your `PATH`.

### Windows

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_INSTALL_PREFIX="C:\Program Files\projot"
cmake --build build --config Release
cmake --install build --config Release
```

Add the install directory to your system `PATH` via **System Properties → Environment Variables**.

---

## Shell completion (planned)

Completion scripts will live in `completions/` once implemented. See [DESIGN.md](DESIGN.md) for the roadmap.

---

## Project layout

```
src/          Core library + main entry point
tests/        doctest test files and fixture data
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
