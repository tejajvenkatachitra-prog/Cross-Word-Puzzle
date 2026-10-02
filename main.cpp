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
#include <set>
static std::unique_ptr<CrosswordGrid> g_grid;
static Trie g_trie;
static std::vector<std::string> g_words;   // the dictionary currently loaded (upper-case, de-duplicated)
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
        "AT", "AN", "AS", "OR", "IF", "TO", "NO", "SO", "GO", "IN", "ON", "OF",
        "IT", "IS", "BE", "WE", "ME", "MY", "UP", "US", "HE", "AM", "AD", "AX",
        "OX",
        "CAT", "CAR", "DOG", "DOT", "ARE", "ART", "TAR", "TEA", "RED", "ROD", "ROT", "ODE",
        "OAT", "EAT", "EAR", "ERA", "ACE", "AGE", "ICE", "ONE", "TWO", "TEN", "SEA", "SUN",
        "SKY", "EGG", "ARC", "ARM", "ASH", "ATE", "AWE", "AXE", "BAD", "BAG", "BAT", "BED",
        "BEE", "BIG", "BIT", "BOX", "BOY", "BUS", "BUT", "BUY", "CAN", "CAP", "COW", "CRY",
        "CUP", "CUT", "DAY", "DEN", "DIG", "DIM", "DIP", "DRY", "DUE", "EEL", "END", "EVE",
        "EYE", "FAN", "FAR", "FAT", "FED", "FEW", "FIG", "FIN", "FIT", "FIX", "FLY", "FOG",
        "FOR", "FOX", "FUN", "GAP", "GAS", "GEM", "GET", "GOT", "GUM", "GUN", "GUT", "GYM",
        "HAT", "HEN", "HER", "HID", "HIM", "HIP", "HIS", "HIT", "HOT", "HOW", "HUB", "HUG",
        "HUT", "ICY", "ILL", "INK", "ION", "IRE", "ITS", "IVY", "JAM", "JAR", "JAW", "JET",
        "JOB", "JOY", "KEY", "KID", "KIT", "LAB", "LAD", "LAP", "LAW", "LAY", "LED", "LEG",
        "LET", "LID", "LIE", "LIP", "LIT", "LOG", "LOT", "LOW", "MAD", "MAN", "MAP", "MAT",
        "MAY", "MEN", "MET", "MIX", "MOB", "MOM", "MUD", "MUG", "NAP", "NET", "NEW", "NOD",
        "NOR", "NOT", "NOW", "NUT", "OAK", "OAR", "OIL", "OLD", "OWL", "OWN", "PAD", "PAN",
        "PAT", "PAW", "PAY", "PEA", "PEN", "PET", "PIE", "PIG", "PIN", "PIT", "POD", "POT",
        "PRO", "PUB", "PUN", "PUP", "RAG", "RAM", "RAN", "RAT", "RAW", "RAY", "RIB", "RID",
        "RIM", "RIP", "ROW", "RUB", "RUG", "RUN", "SAD", "SAT", "SAW", "SAY", "SET", "SEW",
        "SHE", "SIP", "SIR", "SIT", "SIX", "SKI", "SLY", "SOB", "SON", "SPY", "STY", "SUM",
        "TAB", "TAG", "TAN", "TAP", "TAX", "THE", "TIE", "TIN", "TIP", "TOE", "TON", "TOO",
        "TOP", "TOY", "TRY", "TUB", "TUG", "URN", "USE", "VAN", "VET", "WAR", "WAS", "WAX",
        "WAY", "WEB", "WED", "WET", "WHO", "WHY", "WIG", "WIN", "WIT", "WON", "YAK", "YAM",
        "YES", "YET", "YOU", "ZIP", "ZOO",
        "CARE", "CODE", "CORE", "DOTE", "TEAR", "TREE", "ROTE", "ROSE", "ROPE", "RATE", "GATE", "GAZE",
        "MAZE", "MATE", "DATE", "DARE", "BARE", "BAKE", "CAKE", "CANE", "LANE", "LATE", "LACE", "RACE",
        "RICE", "NICE", "MICE", "VICE", "VINE", "WINE", "WIRE", "FIRE", "HIRE", "HIVE", "HAVE", "GAVE",
        "CAVE", "PAVE", "SAVE", "GOLD", "BLUE", "ACRE", "AREA", "ARIA", "ARTS", "AUNT", "BACK", "BALL",
        "BAND", "BANK", "BARN", "BASE", "BATH", "BEAD", "BEAM", "BEAN", "BEAR", "BEAT", "BEEF", "BELL",
        "BELT", "BEND", "BEST", "BIKE", "BIRD", "BITE", "BLOW", "BOAT", "BODY", "BOLD", "BOLT", "BOND",
        "BONE", "BOOK", "BOOT", "BORN", "BOSS", "BOWL", "BULB", "BURN", "BUSY", "CALL", "CALM", "CAMP",
        "CARD", "CART", "CASE", "CASH", "CAST", "CELL", "CHAT", "CHIP", "CITY", "CLAP", "CLAY", "CLIP",
        "CLUB", "COAL", "COAT", "COIL", "COIN", "COLD", "COOK", "COOL", "COPY", "CORN", "COST", "CROP",
        "CROW", "CUBE", "CURL", "DARK", "DASH", "DAWN", "DEAL", "DEAR", "DEEP", "DESK", "DIET", "DIRT",
        "DISH", "DIVE", "DOLL", "DOOR", "DOVE", "DOWN", "DRAW", "DREW", "DROP", "DRUM", "DUCK", "DUST",
        "EACH", "EARN", "EASE", "EAST", "EASY", "EDGE", "ELSE", "EVEN", "EVER", "FACE", "FACT", "FADE",
        "FAIR", "FALL", "FARM", "FAST", "FATE", "FEAR", "FEED", "FEEL", "FELL", "FILE", "FILL", "FILM",
        "FIND", "FINE", "FISH", "FIST", "FLAG", "FLAT", "FLOW", "FOAM", "FOLD", "FOOD", "FOOT", "FORK",
        "FORM", "FORT", "FREE", "FROG", "FUEL", "FULL", "GAIN", "GAME", "GIFT", "GIRL", "GLAD", "GLOW",
        "GLUE", "GOAT", "GOES", "GRAB", "GRAY", "GREW", "GRID", "GRIN", "GROW", "HAIR", "HALF", "HALL",
        "HAND", "HARD", "HARM", "HATE", "HEAD", "HEAL", "HEAR", "HEAT", "HEEL", "HELP", "HERB", "HERO",
        "HIGH", "HILL", "HINT", "HOLD", "HOLE", "HOME", "HOPE", "HORN", "HOST", "HOUR", "HUGE", "HUNT",
        "HURT", "IDEA", "INCH", "IRON", "ITEM", "JAZZ", "JOIN", "JOKE", "JUMP", "JUNE", "JURY", "KEEN",
        "KEEP", "KICK", "KIND", "KING", "KISS", "KITE", "KNEE", "KNOT", "LACK", "LAKE", "LAMB", "LAMP",
        "LAND", "LAST", "LAZY", "LEAD", "LEAF", "LEAN", "LEFT", "LEND", "LESS", "LIFE", "LIFT", "LIKE",
        "LINE", "LINK", "LION", "LIST", "LIVE", "LOAD", "LOAF", "LOAN", "LOCK", "LONG", "LOOK", "LOOP",
        "LORD", "LOSE", "LOST", "LOUD", "LOVE", "LUCK", "MAIL", "MAIN", "MAKE", "MALL", "MANY", "MARK",
        "MASK", "MASS", "MEAL", "MEAN", "MEAT", "MEET", "MELT", "MENU", "MESS", "MILD", "MILE", "MILK",
        "MILL", "MIND", "MINE", "MINT", "MISS", "MOON", "MORE", "MOST", "MOVE", "MUCH", "MUST", "NAME",
        "NAVY", "NEAR", "NECK", "NEED", "NEST", "NEWS", "NEXT", "NOSE", "NOTE", "OPEN", "OVER", "PACE",
        "PACK", "PAGE", "PAID", "PAIN", "PAIR", "PALE", "PALM", "PARK", "PART", "PAST", "PATH", "PEAK",
        "PEAR", "PEEL", "PILE", "PINE", "PINK", "PIPE", "PLAN", "PLAY", "PLOT", "PLUS", "POEM", "POET",
        "POLE", "POND", "POOL", "POOR", "PORT", "POST", "POUR", "PULL", "PUMP", "PURE", "PUSH", "RAIL",
        "RAIN", "RARE", "READ", "REAL", "REST", "RIDE", "RING", "RISE", "RISK", "ROAD", "ROAR", "ROCK",
        "ROLE", "ROLL", "ROOF", "ROOM", "ROOT", "RULE", "RUSH", "SAFE", "SAIL", "SALT", "SAME", "SAND",
        "SEAL", "SEAT", "SEED", "SEEK", "SEEM", "SELL", "SEND", "SHIP", "SHOE", "SHOP", "SHOT", "SHOW",
        "SICK", "SIDE", "SIGN", "SILK", "SING", "SINK", "SIZE", "SKIN", "SLID", "SLIM", "SLOW", "SNOW",
        "SOAP", "SOFT", "SOIL", "SOLD", "SOLE", "SOME", "SONG", "SOON", "SORT", "SOUL", "SOUP", "SPIN",
        "SPOT", "STAR", "STAY", "STEM", "STEP", "STIR", "STOP", "SUCH", "SUIT", "SURE", "SWIM", "TAIL",
        "TAKE", "TALE", "TALK", "TALL", "TANK", "TAPE", "TASK", "TEAM", "TELL", "TENT", "TERM", "TEST",
        "TEXT", "THAN", "THEN", "THEY", "THIN", "THIS", "TIDE", "TIDY", "TILE", "TIME", "TINY", "TIRE",
        "TOAD", "TOLD", "TOLL", "TONE", "TOOK", "TOOL", "TOWN", "TRAP", "TRIM", "TRIP", "TRUE", "TUBE",
        "TUNE", "TURN", "TWIN", "TYPE", "UNIT", "UPON", "USED", "USER", "VAST", "VERY", "VIEW", "VOTE",
        "WAGE", "WAIT", "WAKE", "WALK", "WALL", "WANT", "WARM", "WASH", "WAVE", "WEAK", "WEAR", "WEEK",
        "WELL", "WENT", "WEST", "WHAT", "WHEN", "WIDE", "WIFE", "WILD", "WILL", "WIND", "WING", "WIRE",
        "WISE", "WISH", "WITH", "WOLF", "WOOD", "WOOL", "WORD", "WORE", "WORK", "YARD", "YARN", "YEAR",
        "ZERO", "ZONE",
        "CRANE", "OLIVE", "CARGO", "ERASE", "GRAPE", "STONE", "PLANE", "CHAIR", "TABLE", "HOUSE", "MOUSE", "BREAD",
        "WATER", "EARTH", "LIGHT", "NIGHT", "MUSIC", "HAPPY", "SMILE", "BRAVE", "SHARP", "QUICK", "GREEN", "ABOUT",
        "ABOVE", "ACTOR", "ADULT", "AFTER", "AGAIN", "AGENT", "AGREE", "AHEAD", "ALARM", "ALBUM", "ALERT", "ALIKE",
        "ALIVE", "ALLOW", "ALONE", "ALONG", "ALTER", "AMONG", "ANGEL", "ANGER", "ANGLE", "ANGRY", "APART", "APPLE",
        "APPLY", "ARENA", "ARGUE", "ARISE", "ARRAY", "ASIDE", "ASSET", "AUDIO", "AVOID", "AWARD", "AWARE", "BADLY",
        "BASIC", "BASIN", "BASIS", "BEACH", "BEGAN", "BEGIN", "BEING", "BELOW", "BENCH", "BIRTH", "BLACK", "BLADE",
        "BLAME", "BLANK", "BLAST", "BLAZE", "BLEND", "BLIND", "BLOCK", "BLOOD", "BLOOM", "BOARD", "BOAST", "BONUS",
        "BOOST", "BOOTH", "BRAIN", "BRAND", "BRICK", "BRIDE", "BRIEF", "BRING", "BROAD", "BROWN", "BRUSH", "BUILD",
        "BUNCH", "BURST", "CABIN", "CABLE", "CANDY", "CARRY", "CATCH", "CAUSE", "CHAIN", "CHALK", "CHARM", "CHART",
        "CHASE", "CHEAP", "CHECK", "CHEST", "CHIEF", "CHILD", "CHINA", "CHOIR", "CLAIM", "CLASS", "CLEAN", "CLEAR",
        "CLIMB", "CLOCK", "CLOSE", "CLOUD", "COACH", "COAST", "COUNT", "COURT", "COVER", "CRAFT", "CRASH", "CRAZY",
        "CREAM", "CRIME", "CROSS", "CROWD", "CROWN", "CRUSH", "CURVE", "CYCLE", "DAILY", "DANCE", "DEATH", "DEPTH",
        "DIARY", "DIRTY", "DOUBT", "DRAFT", "DRAIN", "DRAMA", "DREAM", "DRESS", "DRIFT", "DRINK", "DRIVE", "EAGLE",
        "EARLY", "EIGHT", "ELITE", "EMPTY", "ENEMY", "ENJOY", "ENTER", "ENTRY", "EQUAL", "ERROR", "EVENT", "EVERY",
        "EXACT", "EXIST", "EXTRA", "FAINT", "FAITH", "FALSE", "FANCY", "FAULT", "FEAST", "FIELD", "FIFTH", "FIFTY",
        "FIGHT", "FINAL", "FIRST", "FLAME", "FLASH", "FLEET", "FLOOR", "FLOUR", "FLUID", "FOCUS", "FORCE", "FORTH",
        "FORTY", "FRAME", "FRESH", "FRONT", "FRUIT", "FUNNY", "GHOST", "GIANT", "GLASS", "GLOBE", "GLORY", "GRACE",
        "GRADE", "GRAIN", "GRAND", "GRANT", "GRASS", "GRAVE", "GREAT", "GROUP", "GUARD", "GUESS", "GUEST", "GUIDE",
        "HEART", "HEAVY", "HORSE", "HOTEL", "HUMAN", "HUMOR", "IDEAL", "IMAGE", "INDEX", "INNER", "INPUT", "ISSUE",
        "IVORY", "JEWEL", "JOINT", "JUDGE", "JUICE", "KNIFE", "KNOCK", "LABEL", "LARGE", "LASER", "LATER", "LAUGH",
        "LAYER", "LEARN", "LEASE", "LEAST", "LEAVE", "LEGAL", "LEMON", "LEVEL", "LIMIT", "LINEN", "LIVER", "LOCAL",
        "LODGE", "LOOSE", "LOVER", "LOWER", "LUCKY", "LUNCH", "MAGIC", "MAJOR", "MAKER", "MANOR", "MAPLE", "MARCH",
        "MATCH", "MAYBE", "MAYOR", "MEANT", "MEDAL", "MERCY", "MERIT", "METAL", "METER", "MIGHT", "MINOR", "MIXED",
        "MODEL", "MONEY", "MONTH", "MORAL", "MOTOR", "MOUNT", "MOUTH", "MOVIE", "NEVER", "NOBLE", "NOISE", "NORTH",
        "NOVEL", "NURSE", "OCEAN", "OFFER", "OFTEN", "OPERA", "ORDER", "OTHER", "OUTER", "OWNER", "PAINT", "PANEL",
        "PAPER", "PARTY", "PASTA", "PATCH", "PAUSE", "PEACE", "PEARL", "PENNY", "PHASE", "PHONE", "PHOTO", "PIANO",
        "PIECE", "PILOT", "PITCH", "PIZZA", "PLACE", "PLAIN", "PLANT", "PLATE", "PLAZA", "POINT", "POUND", "POWER",
        "PRESS", "PRICE", "PRIDE", "PRIME", "PRINT", "PRIOR", "PRIZE", "PROOF", "PROUD", "PROVE", "QUEEN", "QUIET",
        "RADIO", "RAISE", "RANCH", "RANGE", "RAPID", "RATIO", "REACH", "READY", "REALM", "REFER", "RELAX", "REPLY",
        "RIDER", "RIDGE", "RIGHT", "RIVER", "ROBOT", "ROCKY", "ROMAN", "ROUGH", "ROUND", "ROUTE", "ROYAL", "RURAL",
        "SALAD", "SAUCE", "SCALE", "SCENE", "SCOPE", "SCORE", "SENSE", "SERVE", "SEVEN", "SHADE", "SHAKE", "SHALL",
        "SHAPE", "SHARE", "SHEEP", "SHEET", "SHELF", "SHELL", "SHIFT", "SHINE", "SHIRT", "SHOCK", "SHOOT", "SHORE",
        "SHORT", "SHOWN", "SIGHT", "SINCE", "SIXTH", "SKILL", "SLEEP", "SLICE", "SLIDE", "SMALL", "SMART", "SMELL",
        "SMOKE", "SNAKE", "SOLID", "SOLVE", "SORRY", "SOUND", "SOUTH", "SPACE", "SPARE", "SPEAK", "SPEED", "SPEND",
        "SPICE", "SPLIT", "SPOKE", "SPORT", "STAFF", "STAGE", "STAIR", "STAKE", "STAMP", "STAND", "START", "STATE",
        "STEAM", "STEEL", "STICK", "STILL", "STOCK", "STORE", "STORM", "STORY", "STOVE", "STRIP", "STUDY", "STUFF",
        "STYLE", "SUGAR", "SUITE", "SUNNY", "SUPER", "SWEET", "SWIFT", "SWING", "SWORD", "TASTE", "TEACH", "TEETH",
        "THANK", "THEME", "THERE", "THICK", "THING", "THINK", "THIRD", "THOSE", "THREE", "THROW", "THUMB", "TIGER",
        "TIGHT", "TIRED", "TITLE", "TODAY", "TOOTH", "TOPIC", "TORCH", "TOTAL", "TOUCH", "TOUGH", "TOWEL", "TOWER",
        "TRACE", "TRACK", "TRADE", "TRAIL", "TRAIN", "TREAT", "TREND", "TRIAL", "TRIBE", "TRICK", "TRUCK", "TRULY",
        "TRUST", "TRUTH", "TWICE", "UNCLE", "UNDER", "UNION", "UNITY", "UNTIL", "UPPER", "UPSET", "URBAN", "USAGE",
        "USUAL", "VALID", "VALUE", "VIDEO", "VIRUS", "VISIT", "VITAL", "VOICE", "WAGON", "WASTE", "WATCH", "WHEAT",
        "WHEEL", "WHERE", "WHICH", "WHILE", "WHITE", "WHOLE", "WHOSE", "WOMAN", "WORLD", "WORRY", "WORTH", "WOULD",
        "WRITE", "WRONG", "YOUNG", "YOUTH",
        "ORANGE", "YELLOW", "PURPLE", "SILVER", "GOLDEN", "GARDEN", "WINDOW", "BASKET", "ANCHOR", "MARKET", "FLOWER", "ISLAND",
        "CASTLE", "ENGINE", "PLANET", "FRIEND", "ANIMAL",
    };
}
static void loadGrid(const std::vector<std::string>& rows) {
    g_grid = std::make_unique<CrosswordGrid>(rows);
}
static void loadDictionary(const std::vector<std::string>& words) {
    g_trie = Trie();
    g_words.clear();
    std::set<std::string> seen;
    for (const auto& w : words) {
        std::string upper;
        for (char ch : w) {
            if (std::isalpha(static_cast<unsigned char>(ch)))
                upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        }
        if (upper.size() < 2) continue;              // slots are always >= 2 letters
        if (!seen.insert(upper).second) continue;    // skip duplicates
        g_trie.insert(upper);
        g_words.push_back(upper);
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
        if (rows.size() > 30) {
            res.status = 400;
            res.set_content("{\"error\":\"grid too tall (max 30 rows)\"}", "application/json");
            return;
        }
        for (const auto& r : rows) {
            if (r.size() > 30) {
                res.status = 400;
                res.set_content("{\"error\":\"grid too wide (max 30 columns)\"}", "application/json");
                return;
            }
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
        res.set_content("{\"status\":\"ok\",\"wordCount\":" + std::to_string(g_words.size()) + "}",
                         "application/json");
    });
    svr.Get("/api/dictionary", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        res.set_content("{\"words\":" + stringsToJson(g_words) + ",\"wordCount\":" +
                         std::to_string(g_words.size()) + "}", "application/json");
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
