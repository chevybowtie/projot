#include "process.h"
#include "utils.h"

#include <chrono>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <cerrno>
#  include <csignal>
#  include <fcntl.h>
#  include <sys/wait.h>
#  include <time.h>
#  include <unistd.h>
#endif

#ifdef _WIN32

ProcessResult run_process(const std::vector<std::string>& argv,
                          ChildOutput stdout_mode,
                          ChildOutput stderr_mode,
                          int timeout_ms) {
    ProcessResult result;
    if (argv.empty()) return result;

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
        // Only the child's end may be inherited, or ReadFile never sees EOF.
        SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = capture ? write_end
                  : stdout_mode == ChildOutput::Discard ? null_handle
                  : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = stderr_mode == ChildOutput::Inherit ? GetStdHandle(STD_ERROR_HANDLE) : null_handle;

    PROCESS_INFORMATION pi{};
    const BOOL created = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    // The child holds its own copies now.
    if (write_end) CloseHandle(write_end);
    if (null_handle != INVALID_HANDLE_VALUE) CloseHandle(null_handle);
    if (!created) {
        if (read_end) CloseHandle(read_end);
        return result;
    }
    result.started = true;

    if (capture) {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(read_end, buf, sizeof(buf), &n, nullptr) && n > 0)
            result.output.append(buf, n);
        CloseHandle(read_end);
    }

    const DWORD wait_ms = timeout_ms < 0 ? INFINITE : static_cast<DWORD>(timeout_ms);
    if (WaitForSingleObject(pi.hProcess, wait_ms) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
        result.timed_out = true;
    } else {
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code)) result.exit_code = static_cast<int>(code);
    }
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

    std::vector<char*> cargv;
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    const bool capture = stdout_mode == ChildOutput::Capture;
    int pipe_fds[2] = {-1, -1};
    if (capture && pipe(pipe_fds) != 0) return result;

    pid_t pid = fork();
    if (pid < 0) {
        if (capture) { close(pipe_fds[0]); close(pipe_fds[1]); }
        return result;
    }
    if (pid == 0) {
        // Child: only async-signal-safe calls until exec.
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

    if (capture) {
        close(pipe_fds[1]);
        char buf[4096];
        for (;;) {
            ssize_t n = read(pipe_fds[0], buf, sizeof(buf));
            if (n > 0) result.output.append(buf, static_cast<std::size_t>(n));
            else if (n == 0 || errno != EINTR) break;
        }
        close(pipe_fds[0]);
    }

    int status = 0;
    if (timeout_ms < 0) {
        if (!wait_blocking(pid, status)) return result;
    } else {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        for (;;) {
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) break;
            if (r < 0 && errno != EINTR) return result;
            if (std::chrono::steady_clock::now() >= deadline) {
                // Kill and reap so the child never outlives projot or lingers as a zombie.
                kill(pid, SIGKILL);
                wait_blocking(pid, status);
                result.timed_out = true;
                return result;
            }
            struct timespec ts{0, 50 * 1000 * 1000}; // 50 ms
            nanosleep(&ts, nullptr);
        }
    }
    if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    return result;
}

#endif
