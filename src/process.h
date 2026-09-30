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

// Runs argv[0] (looked up on PATH) with argv. timeout_ms < 0 waits indefinitely;
// otherwise the child is terminated once the timeout elapses, so it never outlives
// projot. With Capture, stdout is read to EOF before the timeout starts counting.
ProcessResult run_process(const std::vector<std::string>& argv,
                          ChildOutput stdout_mode,
                          ChildOutput stderr_mode,
                          int timeout_ms = -1);
