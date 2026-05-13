#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <ctime>

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
