#pragma once

#include <string>
#include <vector>

// Child-process launching without a shell: fork()+execvp() on POSIX,
// CreateProcess on Windows (each argument quoted with quote_windows_arg()).

enum class ChildOutput {
    Inherit,  // share the parent's stream
    Discard,  // redirect to /dev/null (NUL on Windows)
    Capture   // collect into ProcessResult::output (stdout only)
};

struct ProcessResult {
    bool started   = false;  // the process was created (exec may still have failed: exit 127)
    bool timed_out = false;  // killed after exceeding timeout_ms
    int  exit_code = -1;     // valid when started && !timed_out; -1 if killed by a signal
    std::string output;      // captured stdout when stdout_mode == Capture
};

// Runs argv[0] (looked up on PATH) with argv. timeout_ms < 0 waits indefinitely.
// Otherwise the timeout covers the whole run, including reading captured output, so
// a child that hangs with stdout open still times out. On timeout the child and
// everything it started (its process group on POSIX, its job object on Windows) are
// killed and the child is reaped, so nothing outlives projot.
ProcessResult run_process(const std::vector<std::string>& argv,
                          ChildOutput stdout_mode,
                          ChildOutput stderr_mode,
                          int timeout_ms = -1);
