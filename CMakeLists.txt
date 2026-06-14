cmake_minimum_required(VERSION 3.16)
project(projot VERSION 0.1.14 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pre-release suffix (e.g. beta1, rc2). Pass via -DPROJOT_VERSION_SUFFIX=beta1.
set(PROJOT_VERSION_SUFFIX "" CACHE STRING "Pre-release suffix appended to version string")
if(PROJOT_VERSION_SUFFIX STREQUAL "")
    set(PROJOT_VERSION_FULL "${PROJECT_VERSION}")
else()
    set(PROJOT_VERSION_FULL "${PROJECT_VERSION}-${PROJOT_VERSION_SUFFIX}")
endif()

# Version string baked into binary
add_compile_definitions(PROJOT_VERSION="${PROJOT_VERSION_FULL}")
add_compile_definitions(PROJOT_CONFIG_VERSION=1)

# Cross-platform warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ── Core library (shared by main binary and tests) ──────────────────────────
set(CORE_SOURCES
    src/config.cpp
    src/repo.cpp
    src/todo.cpp
    src/markdown.cpp
    src/renderer.cpp
    src/cli.cpp
    src/commands_shared.cpp
    src/commands_project.cpp
    src/commands_config.cpp
    src/commands_maint.cpp
)

add_library(projot_core STATIC ${CORE_SOURCES})
target_include_directories(projot_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ── Main binary ──────────────────────────────────────────────────────────────
add_executable(projot src/main.cpp)
target_link_libraries(projot PRIVATE projot_core)

install(TARGETS projot RUNTIME DESTINATION bin)
install(FILES mcp/server.js mcp/package.json
        DESTINATION share/projot/mcp)

# Shell completions
install(FILES completions/projot.bash
    DESTINATION share/bash-completion/completions
    RENAME projot)
install(FILES completions/_projot
    DESTINATION share/zsh/vendor-completions)
install(FILES completions/projot.fish
    DESTINATION share/fish/vendor_completions.d)
install(FILES completions/projot.ps1
    DESTINATION share/projot)

# Documentation
install(FILES LICENSE README.md
    DESTINATION share/doc/projot)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

set(TEST_SOURCES
    tests/test_config.cpp
    tests/test_repo_discovery.cpp
    tests/test_markdown_parser.cpp
    tests/test_renderer.cpp
    tests/test_todo_model.cpp
    tests/test_commands.cpp
    tests/test_hook.cpp
    tests/test_versioning.cpp
    tests/test_errors.cpp
)

add_executable(projot_tests ${TEST_SOURCES})
target_include_directories(projot_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(projot_tests PRIVATE projot_core)
target_compile_definitions(projot_tests PRIVATE
    PROJOT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
)

add_test(NAME projot_tests COMMAND projot_tests)
