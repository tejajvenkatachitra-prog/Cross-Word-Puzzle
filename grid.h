#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>

// ------------------------------------------------------------
// Crossword Grid -> Slots -> Constraint Graph
// ------------------------------------------------------------
// Input format: a grid of characters.
//   '#'        -> blocked/black cell
//   '.' or '-' -> empty cell to be filled in
//   A letter   -> a pre-filled hint cell (must stay fixed)
//
// A "slot" is a maximal run of consecutive non-blocked cells,
// horizontally (ACROSS) or vertically (DOWN), of length >= 2.
// This is exactly how real crosswords are structured, and it's
// the same idea as finding maximal runs in a 1D array, just
// applied along rows and columns of a 2D grid.
//
// Two slots that pass through the same cell are "intersecting" --
// whatever letter ends up in that cell must agree for both slots.
// That shared-cell relationship is literally a graph: slots are
// vertices, shared cells are edges. Solving the puzzle means
// assigning a word to every vertex such that every edge's
// constraint (matching letter) is satisfied -- this is a classic
// Constraint Satisfaction Problem (CSP), the same family as
// Sudoku or N-Queens.
// ------------------------------------------------------------

enum class Direction { ACROSS, DOWN };

struct Intersection {
    int otherSlot;      // index of the slot this one crosses
    int posInThis;      // character offset within THIS slot's word
    int posInOther;     // character offset within the OTHER slot's word
};

struct Slot {
    int id;
    Direction dir;
    int row, col;                 // starting cell
    int length;
    std::string pattern;          // current state, e.g. "C.T" ('.' = blank)
    std::vector<Intersection> intersections;
};

class CrosswordGrid {
public:
    CrosswordGrid(const std::vector<std::string>& rows) : rows_(rows) {
        height_ = static_cast<int>(rows.size());
        width_ = height_ > 0 ? static_cast<int>(rows[0].size()) : 0;
        buildSlots();
        buildIntersections();
    }

    int height() const { return height_; }
    int width() const { return width_; }
    bool isBlocked(int r, int c) const {
        if (r < 0 || r >= height_ || c < 0 || c >= width_) return true;
        return rows_[r][c] == '#';
    }
    char hintAt(int r, int c) const {
        char ch = rows_[r][c];
        return std::isalpha(static_cast<unsigned char>(ch)) ? static_cast<char>(std::toupper(ch)) : '.';
    }

    std::vector<Slot>& slots() { return slots_; }
    const std::vector<Slot>& slots() const { return slots_; }

    // Writes `word` into a slot's pattern AND updates every intersecting
    // slot's pattern at the shared cell -- this is constraint propagation,
    // the step that lets the solver detect a dead end early instead of
    // discovering it many moves later. Returns a snapshot of whatever
    // characters were sitting in those intersecting positions beforehand,
    // so the solver can precisely undo this exact assignment later.
    std::vector<char> assign(int slotIdx, const std::string& word) {
        std::vector<char> savedChars;
        savedChars.reserve(slots_[slotIdx].intersections.size());
        for (const auto& x : slots_[slotIdx].intersections) {
            savedChars.push_back(slots_[x.otherSlot].pattern[x.posInOther]);
        }
        slots_[slotIdx].pattern = word;
        for (const auto& x : slots_[slotIdx].intersections) {
            slots_[x.otherSlot].pattern[x.posInOther] = word[x.posInThis];
        }
        return savedChars;
    }

    // Undoes exactly one assign() call: restores this slot's own pattern
    // AND restores every intersecting slot's shared-cell character back
    // to what it was before that assignment.
    void unassign(int slotIdx, const std::string& blankPattern, const std::vector<char>& savedChars) {
        slots_[slotIdx].pattern = blankPattern;
        const auto& inters = slots_[slotIdx].intersections;
        for (size_t i = 0; i < inters.size(); i++) {
            slots_[inters[i].otherSlot].pattern[inters[i].posInOther] = savedChars[i];
        }
    }

private:
    std::vector<std::string> rows_;
    int height_, width_;
    std::vector<Slot> slots_;

    void buildSlots() {
        int nextId = 0;

        // ACROSS slots: scan each row left to right
        for (int r = 0; r < height_; r++) {
            int c = 0;
            while (c < width_) {
                if (isBlocked(r, c)) { c++; continue; }
                int start = c;
                while (c < width_ && !isBlocked(r, c)) c++;
                int len = c - start;
                if (len >= 2) {
                    Slot s;
                    s.id = nextId++;
                    s.dir = Direction::ACROSS;
                    s.row = r; s.col = start; s.length = len;
                    s.pattern = initialPattern(r, start, len, true);
                    slots_.push_back(s);
                }
            }
        }

        // DOWN slots: scan each column top to bottom
        for (int c = 0; c < width_; c++) {
            int r = 0;
            while (r < height_) {
                if (isBlocked(r, c)) { r++; continue; }
                int start = r;
                while (r < height_ && !isBlocked(r, c)) r++;
                int len = r - start;
                if (len >= 2) {
                    Slot s;
                    s.id = nextId++;
                    s.dir = Direction::DOWN;
                    s.row = start; s.col = c; s.length = len;
                    s.pattern = initialPattern(start, c, len, false);
                    slots_.push_back(s);
                }
            }
        }
    }

    std::string initialPattern(int r, int c, int len, bool across) const {
        std::string p(len, '.');
        for (int i = 0; i < len; i++) {
            int rr = across ? r : r + i;
            int cc = across ? c + i : c;
            char h = hintAt(rr, cc);
            if (h != '.') p[i] = h;
        }
        return p;
    }

    void buildIntersections() {
        // Map every (row,col) cell -> which slots pass through it, and at
        // what offset. Then any cell touched by 2+ slots is an intersection.
        std::unordered_map<long long, std::vector<std::pair<int,int>>> cellToSlots;
        auto key = [&](int r, int c) { return static_cast<long long>(r) * 10000 + c; };

        for (auto& s : slots_) {
            for (int i = 0; i < s.length; i++) {
                int rr = s.dir == Direction::ACROSS ? s.row : s.row + i;
                int cc = s.dir == Direction::ACROSS ? s.col + i : s.col;
                cellToSlots[key(rr, cc)].push_back({s.id, i});
            }
        }

        for (auto& entry : cellToSlots) {
            auto& list = entry.second;
            if (list.size() < 2) continue;
            for (size_t i = 0; i < list.size(); i++) {
                for (size_t j = 0; j < list.size(); j++) {
                    if (i == j) continue;
                    int slotA = list[i].first, posA = list[i].second;
                    int slotB = list[j].first, posB = list[j].second;
                    slots_[slotA].intersections.push_back({slotB, posA, posB});
                }
            }
        }
    }
};
