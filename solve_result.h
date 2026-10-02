#pragma once
#include <string>
#include <vector>
struct AttemptLog {
    int slotId;
    std::string wordTried;
    bool accepted;  
};

struct SolveResult {
    bool success = false;
    std::vector<std::string> finalPatterns; 
    std::vector<AttemptLog> log;
    long long attemptsCount = 0;
    long long microseconds = 0;
};
