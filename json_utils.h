#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include "grid.h"
#include "solve_result.h"

inline std::string escapeJson(const std::string& s) {
    std::ostringstream os;
    for (char c : s) {
        if (c == '"' || c == '\\') os << '\\' << c;
        else if (c == '\n') os << "\\n";
        else if (c == '\r') os << "\\r";
        else if (c == '\t') os << "\\t";
        else os << c;
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
       << ",\"rows\":[";
    for (size_t i = 0; i < grid.rows().size(); i++) {
        os << "\"" << escapeJson(grid.rows()[i]) << "\"";
        if (i + 1 < grid.rows().size()) os << ",";
    }
    os << "],\"blocked\":[";
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
       << ",\"timedOut\":" << (res.timedOut ? "true" : "false")
       << ",\"logTruncated\":" << (res.logTruncated ? "true" : "false")
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
// ---- minimal request-body parsing ----
// Tolerates whitespace around ':' and '[' and understands \" \\ \n escapes.
inline std::string extractString(const std::string& body, const std::string& key, const std::string& def) {
    std::string needle = "\"" + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return def;
    pos += needle.size();
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) pos++;
    if (pos >= body.size() || body[pos] != ':') return def;
    pos++;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) pos++;
    if (pos >= body.size() || body[pos] != '"') return def;
    pos++;
    std::string out;
    while (pos < body.size() && body[pos] != '"') {
        if (body[pos] == '\\' && pos + 1 < body.size()) {
            pos++;
            char e = body[pos];
            out.push_back(e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e);
        } else out.push_back(body[pos]);
        pos++;
    }
    return out;
}

inline std::vector<std::string> extractStringArray(const std::string& body, const std::string& key) {
    std::vector<std::string> result;
    std::string needle = "\"" + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return result;
    pos += needle.size();
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) pos++;
    if (pos >= body.size() || body[pos] != ':') return result;
    pos++;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) pos++;
    if (pos >= body.size() || body[pos] != '[') return result;
    pos++;

    while (pos < body.size()) {
        while (pos < body.size() && body[pos] != '"' && body[pos] != ']') pos++;
        if (pos >= body.size() || body[pos] == ']') break;
        pos++; // opening quote
        std::string item;
        while (pos < body.size() && body[pos] != '"') {
            if (body[pos] == '\\' && pos + 1 < body.size()) {
                pos++;
                char e = body[pos];
                item.push_back(e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e);
            } else item.push_back(body[pos]);
            pos++;
        }
        result.push_back(item);
        pos++; // closing quote
    }
    return result;
}

inline std::string stringsToJson(const std::vector<std::string>& v) {
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < v.size(); i++) {
        os << "\"" << escapeJson(v[i]) << "\"";
        if (i + 1 < v.size()) os << ",";
    }
    os << "]";
    return os.str();
}
