#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <filesystem>
#include <fstream>

// True if s contains a CR or LF. Every stored value lives on one line of the config
// or notes file, where a line break would start a new key or a new todo.
inline bool has_line_break(const std::string& s) {
    return s.find_first_of("\r\n") != std::string::npos;
}

// Replace target's contents by writing a sibling temp file and renaming it over the
// target, so an interrupted write (crash, kill, full disk) leaves either the old file
// or the new one, never a truncated one. Returns false and sets error on failure.
inline bool atomic_write_file(const std::filesystem::path& target,
                              const std::string& content,
                              std::string& error) {
    namespace fs = std::filesystem;
    std::error_code ec;

    // Write through a symlink instead of replacing the link with a regular file.
    fs::path dest = target;
    if (fs::is_symlink(target, ec)) {
        dest = fs::canonical(target, ec);
        if (ec) { error = "cannot resolve symlink " + target.string() + ": " + ec.message(); return false; }
    }

    const bool dest_exists = fs::exists(dest, ec);
    if (dest_exists) {
        // rename() would replace a file the user made read-only; a truncating write
        // would have failed, so keep that behaviour. Opening for append changes nothing.
        std::ofstream probe(dest, std::ios::app);
        if (!probe.is_open()) { error = "cannot write " + dest.string(); return false; }
    }

    fs::path tmp = dest;
    tmp += ".projot-tmp";
    {
        std::ofstream f(tmp, std::ios::out | std::ios::trunc);
        if (!f.is_open()) { error = "cannot write " + tmp.string(); return false; }
        f << content;
        f.flush();
        const bool written = f.good();
        f.close();
        if (!written || f.fail()) {
            fs::remove(tmp, ec);
            error = "write error to " + tmp.string();
            return false;
        }
    }

    // Carry the original's mode (e.g. an executable hook) over to the replacement.
    if (dest_exists) {
        const auto perms = fs::status(dest, ec).permissions();
        if (!ec) fs::permissions(tmp, perms, fs::perm_options::replace, ec);
    }

    fs::rename(tmp, dest, ec);
    if (ec) {
        error = "cannot replace " + dest.string() + ": " + ec.message();
        std::error_code ignored;
        fs::remove(tmp, ignored);
        return false;
    }
    return true;
}

// Returns today's date as YYYY-MM-DD.
inline std::string date_today() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[11];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}

// Quotes one argument for a Windows command line following the CommandLineToArgvW
// rules: backslashes only need doubling when they precede a quote, and embedded
// quotes are escaped. Platform-independent so it can be unit tested everywhere;
// only the Windows CreateProcess paths use it.
inline std::string quote_windows_arg(const std::string& arg) {
    std::string out = "\"";
    for (auto it = arg.begin();; ++it) {
        size_t backslashes = 0;
        while (it != arg.end() && *it == '\\') { ++it; ++backslashes; }
        if (it == arg.end()) {
            out.append(backslashes * 2, '\\'); // trailing backslashes precede the closing quote
            break;
        }
        if (*it == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
        } else {
            out.append(backslashes, '\\');
            out.push_back(*it);
        }
    }
    out.push_back('"');
    return out;
}

// Format today's date according to a simple format string.
// Supported tokens: YYYY, MM, DD
inline std::string format_date(const std::string& fmt) {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::string year = std::to_string(tm.tm_year + 1900);
    int mon = tm.tm_mon + 1;
    int day = tm.tm_mday;
    std::string month = (mon < 10 ? "0" + std::to_string(mon) : std::to_string(mon));
    std::string day_s = (day < 10 ? "0" + std::to_string(day) : std::to_string(day));

    std::string out = fmt;
    // Replace tokens (simple, non-overlapping)
    // Move past replaced text to avoid infinite loops if replacement contains the token
    size_t pos = 0;
    while ((pos = out.find("YYYY", pos)) != std::string::npos) {
        out.replace(pos, 4, year);
        pos += year.size();  // Move past the inserted text
    }
    pos = 0;
    while ((pos = out.find("MM", pos)) != std::string::npos) {
        out.replace(pos, 2, month);
        pos += month.size();
    }
    pos = 0;
    while ((pos = out.find("DD", pos)) != std::string::npos) {
        out.replace(pos, 2, day_s);
        pos += day_s.size();
    }
    return out;
}

// Deduplicate a vector while preserving order.
// Removes duplicate elements from v, keeping only the first occurrence of each.
template <typename T>
inline std::vector<T> deduplicate(const std::vector<T>& v) {
    std::vector<T> result;
    for (const auto& item : v) {
        if (std::find(result.begin(), result.end(), item) == result.end())
            result.push_back(item);
    }
    return result;
}
