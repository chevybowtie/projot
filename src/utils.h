#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdio>

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
    size_t pos;
    while ((pos = out.find("YYYY")) != std::string::npos) out.replace(pos, 4, year);
    while ((pos = out.find("MM")) != std::string::npos) out.replace(pos, 2, month);
    while ((pos = out.find("DD")) != std::string::npos) out.replace(pos, 2, day_s);
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
