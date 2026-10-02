#pragma once
#include <string>
#include <sstream>
#include <vector>
#include "grid.h"
#include "solve_result.h"

inline std::string escapeJson(const std::string& s) {
    std::ostringstream os;
    for (char c : s) {
        if (c == '"' || c == '\\') os << '\\';
        os << c;
    }
    return os.str();
}

inline std::string slotsToJson(const std::vector<Slot>& slots) {
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < slots.size(); i++) {
        const auto& s = slots[i];
        os << "{\"id\":" << s.id
           << ",\"dir\":\"" << (s.dir == Direction::ACROSS ? "across" : "down") << "\""
           << ",\"row\":" << s.row << ",\"col\":" << s.col
           << ",\"length\":" << s.length
           << ",\"pattern\":\"" << escapeJson(s.pattern) << "\"}";
        if (i + 1 < slots.size()) os << ",";
    }
    os << "]";
    return os.str();
}

inline std::string gridMetaToJson(const CrosswordGrid& grid) {
    std::ostringstream os;
    os << "{\"height\":" << grid.height() << ",\"width\":" << grid.width()
       << ",\"blocked\":[";
    bool first = true;
    for (int r = 0; r < grid.height(); r++) {
        for (int c = 0; c < grid.width(); c++) {
            if (grid.isBlocked(r, c)) {
                if (!first) os << ",";
                os << "[" << r << "," << c << "]";
                first = false;
            }
        }
    }
    os << "],\"slots\":" << slotsToJson(grid.slots()) << "}";
    return os.str();
}

inline std::string solveResultToJson(const SolveResult& res) {
    std::ostringstream os;
    os << "{\"success\":" << (res.success ? "true" : "false")
       << ",\"attemptsCount\":" << res.attemptsCount
       << ",\"microseconds\":" << res.microseconds
       << ",\"finalPatterns\":[";
    for (size_t i = 0; i < res.finalPatterns.size(); i++) {
        os << "\"" << escapeJson(res.finalPatterns[i]) << "\"";
        if (i + 1 < res.finalPatterns.size()) os << ",";
    }
    os << "],\"log\":[";
    for (size_t i = 0; i < res.log.size(); i++) {
        const auto& a = res.log[i];
        os << "{\"slotId\":" << a.slotId
           << ",\"word\":\"" << escapeJson(a.wordTried) << "\""
           << ",\"accepted\":" << (a.accepted ? "true" : "false") << "}";
        if (i + 1 < res.log.size()) os << ",";
    }
    os << "]}";
    return os.str();
}
inline std::string extractString(const std::string& body, const std::string& key, const std::string& def) {
    std::string pattern = "\"" + key + "\":\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return def;
    pos += pattern.size();
    auto end = body.find("\"", pos);
    return body.substr(pos, end - pos);
}
inline std::vector<std::string> extractStringArray(const std::string& body, const std::string& key) {
    std::vector<std::string> result;
    std::string pattern = "\"" + key + "\":[";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return result;
    pos += pattern.size();
    auto end = body.find("]", pos);
    std::string arr = body.substr(pos, end - pos);

    size_t i = 0;
    while (i < arr.size()) {
        auto q1 = arr.find("\"", i);
        if (q1 == std::string::npos) break;
        auto q2 = arr.find("\"", q1 + 1);
        if (q2 == std::string::npos) break;
        result.push_back(arr.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return result;
}
