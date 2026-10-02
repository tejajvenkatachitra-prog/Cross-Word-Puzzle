#pragma once
#include "grid.h"
#include "trie.h"
#include "solve_result.h"
#include <vector>
#include <unordered_set>
#include <string>
#include <algorithm>
#include <chrono>
#include <cstdint>

// ------------------------------------------------------------
// Crossword Solver: Backtracking CSP with MRV + Forward Checking
// ------------------------------------------------------------
// The puzzle is a Constraint Satisfaction Problem:
//   - Variables:   the slots (across/down word positions)
//   - Domains:     dictionary words of the right length matching
//                  the slot's current fixed letters (via the Trie)
//   - Constraints: intersecting slots must agree on the shared letter,
//                  and no word may be reused elsewhere in the grid
//
// The base algorithm is plain backtracking: pick a slot, try a
// candidate word, recurse, undo if it doesn't lead anywhere. On its
// own that's the same idea as N-Queens or Sudoku-by-brute-force.
//
// Two classic optimizations make it actually fast enough to matter:
//
// 1. MRV (Minimum Remaining Values): rather than solving slots in
//    a fixed order, always solve whichever UNFILLED slot currently
//    has the FEWEST legal candidate words next. Intuitively: fail
//    fast, fail early -- if a slot only has 1-2 possible words left,
//    resolving it now prunes the search tree hard; leaving it for
//    later just means more wasted work above it in the recursion.
//
// 2. Forward checking: the moment a word is placed, we don't wait
//    to discover a dead end several slots later. We immediately
//    check every intersecting slot still has at least one candidate
//    word consistent with the new fixed letter. If any intersecting
//    slot's candidate list becomes empty, we back off THIS word
//    immediately instead of recursing further into a doomed branch.
// ------------------------------------------------------------

class CrosswordSolver {
public:
    CrosswordSolver(CrosswordGrid& grid, const Trie& trie,
                    int timeLimitMs = 5000, size_t maxLogEntries = 3000)
        : grid_(grid), trie_(trie), timeLimitMs_(timeLimitMs), maxLog_(maxLogEntries) {}

    SolveResult solve() {
        auto t0 = std::chrono::steady_clock::now();
        deadline_ = t0 + std::chrono::milliseconds(timeLimitMs_);
        timedOut_ = false;
        SolveResult result;

        // Remember the starting state (blank cells + user hints) so the grid
        // can ALWAYS be put back afterwards. Without this a successful solve
        // would leave the grid filled in and the next "Solve" would instantly
        // report success with 0 attempts.
        std::vector<std::string> initial;
        initial.reserve(grid_.slots().size());
        for (const auto& s : grid_.slots()) initial.push_back(s.pattern);

        // Hints already on the board must themselves be legal (a slot made
        // entirely of hint letters has to be a real word, etc.).
        if (stateIsConsistent(allSlotIndexes())) {
            result.success = backtrack(result);
        }

        result.timedOut = timedOut_ && !result.success;
        for (const auto& s : grid_.slots()) result.finalPatterns.push_back(s.pattern);
        if (!result.success) result.finalPatterns = initial;

        for (size_t i = 0; i < initial.size(); i++) grid_.slots()[i].pattern = initial[i];

        auto t1 = std::chrono::steady_clock::now();
        result.microseconds = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        return result;
    }

private:
    CrosswordGrid& grid_;
    const Trie& trie_;
    int timeLimitMs_;
    size_t maxLog_;
    std::chrono::steady_clock::time_point deadline_;
    bool timedOut_ = false;

    static bool isComplete(const Slot& s) { return s.pattern.find('.') == std::string::npos; }

    std::vector<int> allSlotIndexes() const {
        std::vector<int> v(grid_.slots().size());
        for (size_t i = 0; i < v.size(); i++) v[i] = static_cast<int>(i);
        return v;
    }

    // Every word that is fully spelled out on the board right now (whether
    // the solver placed it or it was completed by crossing letters).
    // Returns false if two slots spell the same word.
    bool collectCompleted(std::unordered_set<std::string>& completed) const {
        completed.clear();
        for (const auto& s : grid_.slots()) {
            if (!isComplete(s)) continue;
            if (!completed.insert(s.pattern).second) return false; // duplicate word
        }
        return true;
    }

    // Dictionary words that could still legally fill this slot:
    // right length, match the fixed letters, and aren't already on the board.
    std::vector<std::string> candidatesFor(const Slot& slot,
                                           const std::unordered_set<std::string>& completed) const {
        std::vector<std::string> raw = trie_.matchPattern(slot.pattern);
        std::vector<std::string> filtered;
        filtered.reserve(raw.size());
        for (auto& w : raw) {
            if (!completed.count(w)) filtered.push_back(std::move(w));
        }
        return filtered;
    }

    // Consistency check for a set of slots:
    //  - a fully-filled slot must be a real dictionary word (this is what
    //    catches a crossing slot that got completed into gibberish),
    //  - no word may appear twice,
    //  - a partially-filled slot must still have >= 1 candidate.
    bool stateIsConsistent(const std::vector<int>& slotIdxs) const {
        std::unordered_set<std::string> completed;
        if (!collectCompleted(completed)) return false;
        for (int i : slotIdxs) {
            const Slot& s = grid_.slots()[i];
            if (isComplete(s)) {
                if (!trie_.contains(s.pattern)) return false;
            } else if (candidatesFor(s, completed).empty()) {
                return false;
            }
        }
        return true;
    }

    // MRV: among all unfilled slots pick the one with the fewest candidates.
    // Ties go to the longer slot. Returns -1 if every slot is filled.
    int pickNextSlot(std::vector<std::string>& outCandidates) const {
        std::unordered_set<std::string> completed;
        collectCompleted(completed);

        int best = -1;
        size_t bestCount = SIZE_MAX;
        for (size_t i = 0; i < grid_.slots().size(); i++) {
            const Slot& s = grid_.slots()[i];
            if (isComplete(s)) continue;
            auto cands = candidatesFor(s, completed);
            bool better = cands.size() < bestCount ||
                          (cands.size() == bestCount && best >= 0 && s.length > grid_.slots()[best].length);
            if (better) {
                bestCount = cands.size();
                best = static_cast<int>(i);
                outCandidates = std::move(cands);
                if (bestCount == 0) break; // dead end: report immediately
            }
        }
        return best;
    }

    // Forward checking: after placing a word, every slot it crosses (plus
    // the global no-duplicates rule) must still be satisfiable.
    bool forwardCheckOk(int slotIdx) const {
        std::vector<int> crossing;
        for (const auto& x : grid_.slots()[slotIdx].intersections) crossing.push_back(x.otherSlot);
        return stateIsConsistent(crossing);
    }

    bool backtrack(SolveResult& result) {
        if (std::chrono::steady_clock::now() > deadline_) { timedOut_ = true; return false; }

        std::vector<std::string> candidates;
        int slotIdx = pickNextSlot(candidates);
        if (slotIdx == -1) return true;       // every slot is filled and was validated on the way
        if (candidates.empty()) return false; // MRV found a dead-end slot -- backtrack now

        std::string savedPattern = grid_.slots()[slotIdx].pattern;
        int slotId = grid_.slots()[slotIdx].id;

        for (const auto& word : candidates) {
            if (timedOut_) return false;
            result.attemptsCount++;

            std::vector<char> savedChars = grid_.assign(slotIdx, word);

            bool ok = forwardCheckOk(slotIdx);
            if (result.log.size() < maxLog_) result.log.push_back({slotId, word, ok});
            else result.logTruncated = true;

            if (ok && backtrack(result)) return true;

            // undo: this word didn't work out (or a deeper slot failed), try the next one
            grid_.unassign(slotIdx, savedPattern, savedChars);
        }
        return false;
    }
};
