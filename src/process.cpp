#include "process.h"
#include "utils.h"

#include <chrono>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <cerrno>
#  include <csignal>
#  include <fcntl.h>
#  include <poll.h>
#  include <sys/wait.h>
#  include <time.h>
#  include <unistd.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

// Deadline for the whole run: capturing output and waiting for exit share it, so a
// child that holds stdout open cannot outlast the timeout.
struct Deadline {
    bool active = false;
    Clock::time_point at;

    explicit Deadline(int timeout_ms)
        : active(timeout_ms >= 0),
          at(Clock::now() + std::chrono::milliseconds(timeout_ms < 0 ? 0 : timeout_ms)) {}

    // Milliseconds left, clamped at 0; -1 when there is no deadline.
    int remaining_ms() const {
        if (!active) return -1;
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(at - Clock::now()).count();
        return left > 0 ? static_cast<int>(left) : 0;
    }
    bool expired() const { return active && remaining_ms() == 0; }
};

constexpr int POLL_INTERVAL_MS = 20;

} // namespace

#ifdef _WIN32

ProcessResult run_process(const std::vector<std::string>& argv,
                          ChildOutput stdout_mode,
                          ChildOutput stderr_mode,
                          int timeout_ms) {
    ProcessResult result;
    if (argv.empty()) return result;
    const Deadline deadline(timeout_ms);

    std::string cmd;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) cmd += ' ';
        cmd += quote_windows_arg(argv[i]);
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    const bool capture = stdout_mode == ChildOutput::Capture;
    const bool need_null = stdout_mode == ChildOutput::Discard || stderr_mode != ChildOutput::Inherit;
    HANDLE null_handle = INVALID_HANDLE_VALUE;
    if (need_null) {
        null_handle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    HANDLE read_end = nullptr;
    HANDLE write_end = nullptr;
    if (capture) {
        if (!CreatePipe(&read_end, &write_end, &sa, 0)) {
            if (null_handle != INVALID_HANDLE_VALUE) CloseHandle(null_handle);
            return result;
        }
        // Only the child's end may be inherited, or the pipe never reports EOF.
        SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    }

    // With a timeout, run the child in a job so a timeout also kills anything it
    // started. No KILL_ON_JOB_CLOSE: after a normal exit, legitimate background work
    // (e.g. git's auto-gc) must be left alone.
    HANDLE job = deadline.active ? CreateJobObjectA(nullptr, nullptr) : nullptr;

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = capture ? write_end
                  : stdout_mode == ChildOutput::Discard ? null_handle
                  : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = stderr_mode == ChildOutput::Inherit ? GetStdHandle(STD_ERROR_HANDLE) : null_handle;

    PROCESS_INFORMATION pi{};
    // Start suspended when using a job, so the child cannot spawn anything before
    // it has been assigned to the job.
    const DWORD flags = CREATE_NO_WINDOW | (job ? CREATE_SUSPENDED : 0);
    const BOOL created = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                        flags, nullptr, nullptr, &si, &pi);
    // The child holds its own copies now.
    if (write_end) CloseHandle(write_end);
    if (null_handle != INVALID_HANDLE_VALUE) CloseHandle(null_handle);
    if (!created) {
        if (read_end) CloseHandle(read_end);
        if (job) CloseHandle(job);
        return result;
    }
    result.started = true;

    if (job) {
        if (!AssignProcessToJobObject(job, pi.hProcess)) {
            CloseHandle(job);  // fall back to terminating just the child
            job = nullptr;
        }
        ResumeThread(pi.hThread);
    }

    auto terminate = [&] {
        if (job) TerminateJobObject(job, 1);
        else     TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
        result.timed_out = true;
    };

    bool finished = true;
    if (capture) {
        // Anonymous pipes cannot do overlapped I/O, so poll with PeekNamedPipe and
        // read only what is available; a blocking ReadFile would ignore the deadline.
        char buf[4096];
        for (;;) {
            DWORD avail = 0;
            if (!PeekNamedPipe(read_end, nullptr, 0, nullptr, &avail, nullptr)) break;  // all writers closed
            if (avail > 0) {
                DWORD n = 0;
                const DWORD want = avail < sizeof(buf) ? avail : static_cast<DWORD>(sizeof(buf));
                if (!ReadFile(read_end, buf, want, &n, nullptr) || n == 0) break;
                result.output.append(buf, n);
                continue;
            }
            if (deadline.expired()) { finished = false; break; }
            const int left = deadline.remaining_ms();
            Sleep(static_cast<DWORD>(left >= 0 && left < POLL_INTERVAL_MS ? left : POLL_INTERVAL_MS));
        }
        CloseHandle(read_end);
    }

    if (!finished) {
        terminate();
    } else {
        const int left = deadline.remaining_ms();
        const DWORD wait_ms = left < 0 ? INFINITE : static_cast<DWORD>(left);
        if (WaitForSingleObject(pi.hProcess, wait_ms) == WAIT_TIMEOUT) {
            terminate();
        } else {
            DWORD code = 0;
            if (GetExitCodeProcess(pi.hProcess, &code)) result.exit_code = static_cast<int>(code);
        }
    }
    if (job) CloseHandle(job);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return result;
}

#else

// Waits for pid, retrying on EINTR. Returns false if the child could not be reaped.
static bool wait_blocking(pid_t pid, int& status) {
    pid_t r;
    do { r = waitpid(pid, &status, 0); } while (r < 0 && errno == EINTR);
    return r == pid;
}

ProcessResult run_process(const std::vector<std::string>& argv,
                          ChildOutput stdout_mode,
                          ChildOutput stderr_mode,
                          int timeout_ms) {
    ProcessResult result;
    if (argv.empty()) return result;
    const Deadline deadline(timeout_ms);

    std::vector<char*> cargv;
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    const bool capture = stdout_mode == ChildOutput::Capture;
    int pipe_fds[2] = {-1, -1};
    if (capture && pipe(pipe_fds) != 0) return result;

    // With a timeout, give the child its own process group so a timeout kills
    // anything it started too. Only then: a child outside the terminal's foreground
    // group no longer receives Ctrl-C.
    const bool own_group = deadline.active;

    pid_t pid = fork();
    if (pid < 0) {
        if (capture) { close(pipe_fds[0]); close(pipe_fds[1]); }
        return result;
    }
    if (pid == 0) {
        // Child: only async-signal-safe calls until exec.
        if (own_group) setpgid(0, 0);
        if (capture) {
            dup2(pipe_fds[1], STDOUT_FILENO);
            close(pipe_fds[0]);
            close(pipe_fds[1]);
        }
        const bool discard_out = stdout_mode == ChildOutput::Discard;
        const bool discard_err = stderr_mode != ChildOutput::Inherit;
        if (discard_out || discard_err) {
            int fd = open("/dev/null", O_WRONLY);
            if (fd >= 0) {
                if (discard_out) dup2(fd, STDOUT_FILENO);
                if (discard_err) dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        execvp(cargv[0], cargv.data());
        _exit(127);
    }
    result.started = true;
    // Also set the group from the parent, so it is in place before any kill below
    // regardless of which process runs first. Fails harmlessly once the child execs.
    if (own_group) setpgid(pid, pid);

    int status = 0;
    // Kill and reap so the child never outlives projot or lingers as a zombie.
    auto terminate = [&] {
        kill(own_group ? -pid : pid, SIGKILL);
        wait_blocking(pid, status);
        result.timed_out = true;
    };

    if (capture) {
        close(pipe_fds[1]);
        const int fd = pipe_fds[0];
        // Non-blocking reads gated by poll(), so the deadline is checked even while
        // the child (or a descendant that inherited stdout) keeps the pipe open.
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        char buf[4096];
        bool eof = false;
        bool timed_out = false;
        while (!eof) {
            struct pollfd pfd{fd, POLLIN, 0};
            const int pr = poll(&pfd, 1, deadline.remaining_ms());
            if (pr < 0) {
                if (errno == EINTR) continue;
                break;  // unexpected poll failure: stop reading, still reap below
            }
            if (pr == 0) { timed_out = true; break; }
            for (;;) {
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n > 0) { result.output.append(buf, static_cast<std::size_t>(n)); continue; }
                if (n == 0) { eof = true; break; }
                if (errno == EINTR) continue;
                if (errno != EAGAIN && errno != EWOULDBLOCK) eof = true;  // read error: stop
                break;
            }
        }
        close(fd);
        if (timed_out) {
            terminate();
            return result;
        }
    }

    if (!deadline.active) {
        if (!wait_blocking(pid, status)) return result;
    } else {
        for (;;) {
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) break;
            if (r < 0 && errno != EINTR) return result;
            if (deadline.expired()) {
                terminate();
                return result;
            }
            struct timespec ts{0, POLL_INTERVAL_MS * 1000L * 1000L};
            nanosleep(&ts, nullptr);
        }
    }
    if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    return result;
}

#endif
