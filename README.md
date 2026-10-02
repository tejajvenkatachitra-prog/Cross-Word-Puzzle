# Crosswise — Crossword Constraint Solver

An interactive crossword solver. Draw a black/white grid layout, load a
word list, and watch a C++ backend fill it in using a Trie-backed
backtracking search — with the whole attempt-by-attempt process animated
live in the browser.

---

## 1. Why this project shows a lot of DSA

| Concept | File | What it demonstrates |
|---|---|---|
| Trie (prefix tree) | `trie.h` | Stores the dictionary; supports wildcard pattern matching (`"C.T"` → CAT, COT, CUT...) |
| Constraint graph | `grid.h` | Crossword slots that share a cell are modeled as graph vertices/edges |
| Backtracking / recursion | `csp_solver.h` | Core search algorithm — same family as N-Queens / Sudoku solvers |
| MRV heuristic | `csp_solver.h` | Always solves the slot with the *fewest* legal words left first — fails fast, prunes hard |
| Forward checking | `csp_solver.h` | The instant a word is placed, every crossing slot is re-checked for at least one valid option — catches dead ends immediately instead of many moves later |
| Hashing (hash set) | `csp_solver.h` | Tracks used words in O(1) so no word is placed twice |

**The live demo story for your instructor:**
1. Show the grid — every empty run of 2+ cells is a "slot"; slots that cross share a letter constraint.
2. Explain the Trie — it's what makes "find every 5-letter word matching `CR_N_`" fast instead of scanning the whole dictionary.
3. Run the solver on the default puzzle — watch cells flash amber (trying), then teal (accepted) or red (backtracked).
4. Point at the stats panel: "attempts" is how many words the backtracking loop had to actually try, not just how many exist.
5. Load a different, deliberately too-small dictionary and re-solve, to show the MRV heuristic detecting an impossible slot **instantly** (0 wasted attempts) rather than blindly searching.

---

## 2. Project structure (deliberately flat — no subfolders)

```
crossword-solver/
├── main.cpp             # HTTP server + routes (uses cpp-httplib)
├── httplib.h             # vendored single-header HTTP library
├── json_utils.h          # tiny JSON (de)serialization helpers
├── trie.h                 # Trie data structure + wildcard search
├── grid.h                 # grid parsing, slot detection, constraint graph
├── solve_result.h         # shared result struct
├── csp_solver.h            # the backtracking CSP solver (MRV + forward checking)
├── index.html
├── style.css
├── script.js
├── Dockerfile
├── render.yaml
└── .gitignore
```

Everything sits in one folder on purpose — this avoids the GitHub-web-upload
folder-flattening issue some people hit when dragging files in one at a time
instead of using `git push`. If you're uploading manually, you can safely
drag every file in this folder straight into the repo root.

The C++ server does double duty here too: it serves the frontend's static
files **and** the algorithm API, so it deploys as a single Render web
service.

---

## 3. Running it locally

```bash
g++ -std=c++17 -O2 -pthread main.cpp -o server
mkdir -p public
cp index.html style.css script.js public/
./server
```

Open `http://localhost:8080`.

---

## 4. Pushing to GitHub

```bash
cd crossword-solver
git init
git add .
git commit -m "Crossword solver: Trie + CSP backtracking with MRV and forward checking"
git branch -M main
git remote add origin https://github.com/<your-username>/crossword-solver.git
git push -u origin main
```

---

## 5. Deploying on Render

Same process as before:

1. Render dashboard → **New → Blueprint** → connect this repo → Render reads `render.yaml` automatically → **Apply**.
2. Or manually: **New → Web Service** → connect repo → runtime **Docker** (auto-detected from the `Dockerfile`) → **Free** plan → **Create Web Service**.
3. Wait for the build (installs build tools, compiles `main.cpp`, copies the frontend into `./public`).
4. Open the given `.onrender.com` URL once it's live.

### Environment variables

**None required.** The only one the code reads is `PORT`, and Render
injects that automatically for every web service — you don't set it
yourself. There's no database, API key, or secret anywhere in this project.
If Render's setup screen shows an Environment tab, you can leave it empty.

### Free tier note

Same as before: Render's free web services spin down after ~15 minutes of
inactivity and take 30-50 seconds to wake on the next request. Open the URL
a minute or two before your demo so it's already warm.

---

## 6. Explaining the code — talking points

- **Why a Trie instead of scanning the word list every time:** matching a pattern like `C.T` against a list of thousands of words means checking every single word's length and letters — O(n × length). A Trie instead walks down one path per fixed letter and only branches (into all 26 children) at wildcard positions, so it only explores paths that could possibly match, not the whole dictionary.
- **Why backtracking:** there's no formula that jumps straight to a solved crossword. You have to try a word, see if it's still consistent everywhere it crosses another slot, and undo it if it isn't. That try/check/undo loop is exactly what recursion + backtracking gives you for free.
- **Why MRV (Minimum Remaining Values):** if you solved slots in a fixed left-to-right order, you might place four easy words before discovering the fifth slot has zero valid options left — wasting all the work on those first four. By always tackling whichever slot is most constrained *right now*, dead ends get discovered as early as possible.
- **Why forward checking:** without it, you'd only discover a dead end when you actually try to solve the choked-off slot, potentially many recursive calls later. Checking immediately after every placement catches it the moment it happens.
- **Why a hash set for used words:** crosswords conventionally don't repeat the same word twice. Checking membership in a `std::unordered_set` is O(1) average case, so this costs effectively nothing per candidate check.

---

## 7. Possible extensions if you want to go further

- Load a bigger real-world dictionary (e.g. the classic `/usr/share/dict/words` word list, ~100k+ words) for more impressive-looking solves
- Add clue text per slot (Across/Down numbered clues, like a real newspaper crossword)
- Add a "generate empty grid" mode that also *designs* the block layout, not just solves a given one
- Track and display the *maximum recursion depth* reached, as another complexity talking point
