#include "httplib.h"
#include "json_utils.h"
#include "trie.h"
#include "grid.h"
#include "csp_solver.h"
#include <memory>
#include <mutex>
#include <cstdlib>
#include <iostream>
#include <algorithm>
static std::unique_ptr<CrosswordGrid> g_grid;
static Trie g_trie;
static std::mutex g_mutex;
static std::vector<std::string> defaultRows() {
    return {
        ".....",
        ".###.",
        ".###.",
        ".###.",
        "....."
    };
}
static std::vector<std::string> defaultWords() {
    return {
        "AT","AN","AS","OR","IF","TO","NO","SO","GO","IN","ON","OF","IT","IS","BE","WE",
        "CAT","CAR","DOG","DOT","ARE","ART","TAR","TEA","RED","ROD","ROT","ODE","OAT",
        "EAT","EAR","ERA","ACE","AGE","ICE","ONE","TWO","TEN","SEA","SUN","SKY","EGG",
        "CARE","CODE","CORE","DOTE","TEAR","TREE","ROTE","ROSE",
        "ROPE","RATE","GATE","GAZE","MAZE","MATE","DATE","DARE","BARE","BAKE","CAKE",
        "CANE","LANE","LATE","LACE","RACE","RICE","NICE","MICE","VICE","VINE","WINE",
        "WIRE","FIRE","HIRE","HIVE","HAVE","GAVE","CAVE","PAVE","SAVE","GOLD","BLUE",
        "CRANE","OLIVE","CARGO","ERASE","GRAPE","STONE","PLANE","CHAIR","TABLE","HOUSE",
        "MOUSE","BREAD","WATER","EARTH","LIGHT","NIGHT","MUSIC","HAPPY","SMILE","BRAVE",
        "SHARP","QUICK","GREEN",
        "ORANGE","YELLOW","PURPLE","SILVER","GOLDEN","GARDEN","WINDOW","BASKET","ANCHOR",
        "MARKET","FLOWER","ISLAND","CASTLE","ENGINE","PLANET","FRIEND","ANIMAL"
    };
}
static void loadGrid(const std::vector<std::string>& rows) {
    g_grid = std::make_unique<CrosswordGrid>(rows);
}
static void loadDictionary(const std::vector<std::string>& words) {
    g_trie = Trie();
    for (const auto& w : words) {
        std::string upper = w;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        g_trie.insert(upper);
    }
}
int main() {
    loadGrid(defaultRows());
    loadDictionary(defaultWords());

    httplib::Server svr;

    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    svr.set_mount_point("/", "./public");

    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    svr.Get("/api/state", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        res.set_content(gridMetaToJson(*g_grid), "application/json");
    });
    svr.Post("/api/grid", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto rows = extractStringArray(req.body, "rows");
        if (rows.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"no rows provided\"}", "application/json");
            return;
        }
        loadGrid(rows);
        res.set_content(gridMetaToJson(*g_grid), "application/json");
    });
    svr.Post("/api/dictionary", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto words = extractStringArray(req.body, "words");
        if (words.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"no words provided\"}", "application/json");
            return;
        }
        loadDictionary(words);
        res.set_content("{\"status\":\"ok\",\"wordCount\":" + std::to_string(words.size()) + "}",
                         "application/json");
    });

    svr.Post("/api/solve", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        CrosswordSolver solver(*g_grid, g_trie);
        SolveResult result = solver.solve();
        res.set_content(solveResultToJson(result), "application/json");
    });

    svr.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::atoi(portEnv) : 8080;

    std::cout << "Crossword Solver server listening on port " << port << std::endl;
    svr.listen("0.0.0.0", port);

    return 0;
}
