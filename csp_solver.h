#pragma once
#include "grid.h"
#include "trie.h"
#include "solve_result.h"
#include <vector>
#include <unordered_set>
#include <string>
#include <algorithm>
#include <chrono>

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
    CrosswordSolver(CrosswordGrid& grid, const Trie& trie)
        : grid_(grid), trie_(trie) {}

    SolveResult solve() {
        auto t0 = std::chrono::high_resolution_clock::now();
        SolveResult result;

        std::vector<bool> assigned(grid_.slots().size(), false);
        result.success = backtrack(assigned, result);

        result.finalPatterns.reserve(grid_.slots().size());
        for (const auto& s : grid_.slots()) result.finalPatterns.push_back(s.pattern);

        auto t1 = std::chrono::high_resolution_clock::now();
        result.microseconds = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        return result;
    }

private:
    CrosswordGrid& grid_;
    const Trie& trie_;
    std::unordered_set<std::string> usedWords_;

    // Returns every dictionary word that could still legally fill this slot:
    // right length, matches already-fixed letters, and isn't used elsewhere.
    std::vector<std::string> candidatesFor(const Slot& slot) {
        std::vector<std::string> raw = trie_.matchPattern(slot.pattern);
        std::vector<std::string> filtered;
        filtered.reserve(raw.size());
        for (auto& w : raw) {
            if (!usedWords_.count(w)) filtered.push_back(w);
        }
        return filtered;
    }

    // MRV heuristic: among all not-yet-fully-fixed slots, pick the one
    // with the fewest legal candidate words. Ties broken by longer slots
    // first (longer words are rarer, so they're worth locking in early too).
    int pickNextSlot(const std::vector<bool>& assigned, std::vector<std::string>& outCandidates) {
        int best = -1;
        size_t bestCount = SIZE_MAX;

        for (size_t i = 0; i < grid_.slots().size(); i++) {
            if (assigned[i]) continue;
            if (grid_.slots()[i].pattern.find('.') == std::string::npos) {
                // already fully determined by intersections; just needs bookkeeping
                continue;
            }
            auto cands = candidatesFor(grid_.slots()[i]);
            if (cands.size() < bestCount) {
                bestCount = cands.size();
                best = static_cast<int>(i);
                outCandidates = cands;
                if (bestCount == 0) break; // can't do better than a dead end; report it immediately
            }
        }
        return best;
    }

    bool allSlotsComplete(const std::vector<bool>& assigned) {
        for (size_t i = 0; i < grid_.slots().size(); i++) {
            if (grid_.slots()[i].pattern.find('.') != std::string::npos) return false;
        }
        return true;
    }

    // Forward checking: after placing `word`, verify every intersecting
    // slot still has >= 1 viable candidate. If any is already choked off,
    // this placement is doomed -- report failure immediately rather than
    // recursing deeper into a branch that can never succeed.
    bool forwardCheckOk(int slotIdx) {
        for (const auto& x : grid_.slots()[slotIdx].intersections) {
            const Slot& other = grid_.slots()[x.otherSlot];
            if (other.pattern.find('.') == std::string::npos) continue; // already fixed, nothing to check
            if (candidatesFor(other).empty()) return false;
        }
        return true;
    }

    bool backtrack(std::vector<bool>& assigned, SolveResult& result) {
        if (allSlotsComplete(assigned)) return true;

        std::vector<std::string> candidates;
        int slotIdx = pickNextSlot(assigned, candidates);
        if (slotIdx == -1) return true;       // nothing left needing a word choice
        if (candidates.empty()) return false; // MRV found a dead-end slot -- backtrack now

        std::string savedPattern = grid_.slots()[slotIdx].pattern;

        for (const auto& word : candidates) {
            result.attemptsCount++;

            std::vector<char> savedChars = grid_.assign(slotIdx, word);
            usedWords_.insert(word);
            assigned[slotIdx] = true;

            bool ok = forwardCheckOk(slotIdx);
            if (ok) {
                result.log.push_back({grid_.slots()[slotIdx].id, word, true});
                if (backtrack(assigned, result)) return true;
            } else {
                result.log.push_back({grid_.slots()[slotIdx].id, word, false});
            }

            // undo: this word didn't work out (or a deeper slot failed), try the next candidate
            usedWords_.erase(word);
            assigned[slotIdx] = false;
            grid_.unassign(slotIdx, savedPattern, savedChars);
        }

        return false;
    }
};
