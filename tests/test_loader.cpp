// File:       tests/test_loader.cpp
// Owner:      Rabia
// Purpose:    Tests for HashMap and ScenarioLoader (tokenizer, parseInt, the
//             three invalid files, the valid scenarios). Own main(), own target.
// Depends on: HashMap, ScenarioLoader, World
// Used by:    ctest / "before a PR" check
// Milestone:  M1 (HashMap), M2 (loader)
//
// RESQGRID_DATA_DIR is set by CMake to <repo>/data, so tests find the files
// whatever folder they run from.
#include <iostream>
#include <fstream>
#include "containers/HashMap.h"
#include "core/ScenarioLoader.h"
#include "core/World.h"

static int g_checks = 0, g_failed = 0, g_todo = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; \
    cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)
#define TODO_TEST(name) do { ++g_todo; cout << "  TODO  " << name << "\n"; } while (0)

static Str dataFile(const Str& name) { return Str(RESQGRID_DATA_DIR) + "/" + name; }

static void testHashMap() {
    // empty map: nothing found, and find before init() must not crash
    HashMap<int> empty;
    int v = -5;
    CHECK(!empty.find(3, v));
    CHECK(!empty.contains(3));
    CHECK(empty.size() == 0);

    // bucket count is a prime >= 2 * expected
    HashMap<int> m;
    m.init(100);
    CHECK(m.bucketCount() == 211);
    HashMap<int> tiny;
    tiny.init(0);
    CHECK(tiny.bucketCount() == 2);

    // insert 1..100, value = key * 10
    bool allIn = true;
    for (int i = 1; i <= 100; i++) {
        if (!m.insert(i, i * 10)) {
            allIn = false;
        }
    }
    CHECK(allIn);
    CHECK(m.size() == 100);

    // duplicate insert is refused and does not overwrite
    CHECK(!m.insert(50, 999));
    CHECK(m.find(50, v) && v == 500);
    CHECK(m.size() == 100);

    // every key found with its value
    bool allFound = true;
    for (int i = 1; i <= 100; i++) {
        if (!m.find(i, v) || v != i * 10) {
            allFound = false;
        }
    }
    CHECK(allFound);
    CHECK(!m.find(1000, v));
    CHECK(!m.contains(0));

    // non-consecutive keys, keys in the same bucket, negative keys
    HashMap<int> s;
    s.init(3);                       // 7 buckets
    CHECK(s.insert(1, 11));
    CHECK(s.insert(7, 77));
    CHECK(s.insert(1000, 5));
    CHECK(s.insert(8, 88));          // same bucket as 1 (8 mod 7 = 1)
    CHECK(s.insert(-3, 33));
    CHECK(s.insert(-10, 100));       // same bucket as -3
    CHECK(!s.insert(-3, 0));
    CHECK(s.find(1, v) && v == 11);
    CHECK(s.find(8, v) && v == 88);
    CHECK(s.find(1000, v) && v == 5);
    CHECK(s.find(-3, v) && v == 33);
    CHECK(s.find(-10, v) && v == 100);
    CHECK(!s.find(-4, v));

    // single element
    HashMap<int> one;
    one.init(1);
    CHECK(one.insert(42, 0));
    CHECK(one.contains(42));
    CHECK(one.size() == 1);

    // clear keeps the buckets, removes the keys
    s.clear();
    CHECK(s.size() == 0);
    CHECK(s.bucketCount() == 7);
    CHECK(!s.contains(1));
    CHECK(s.insert(1, 12));
    CHECK(s.find(1, v) && v == 12);
}

static void testTokenizeAndParseInt() {
    Str t[ScenarioLoader::MAX_TOKENS];
    CHECK(ScenarioLoader::tokenize("  1   Junction_1 JUNCTION ", t, ScenarioLoader::MAX_TOKENS) == 3);
    CHECK(t[0] == "1" && t[1] == "Junction_1" && t[2] == "JUNCTION");
    CHECK(ScenarioLoader::tokenize("", t, ScenarioLoader::MAX_TOKENS) == 0);
    CHECK(ScenarioLoader::tokenize("    ", t, ScenarioLoader::MAX_TOKENS) == 0);
    CHECK(ScenarioLoader::tokenize("\ta\tb  \r", t, ScenarioLoader::MAX_TOKENS) == 2);  // tabs, Windows line end
    CHECK(t[0] == "a" && t[1] == "b");
    CHECK(ScenarioLoader::tokenize("x", t, ScenarioLoader::MAX_TOKENS) == 1 && t[0] == "x");
    CHECK(ScenarioLoader::tokenize("a b c d", t, 2) == 2);                             // stops at maxTokens

    int v = 0;
    CHECK(ScenarioLoader::parseInt("42", v) && v == 42);
    CHECK(ScenarioLoader::parseInt("-3", v) && v == -3);
    CHECK(ScenarioLoader::parseInt("0", v) && v == 0);
    CHECK(ScenarioLoader::parseInt("2147483647", v) && v == 2147483647);
    CHECK(ScenarioLoader::parseInt("-2147483648", v) && v == -2147483647 - 1);
    v = 99;
    CHECK(!ScenarioLoader::parseInt("", v));
    CHECK(!ScenarioLoader::parseInt("-", v));
    CHECK(!ScenarioLoader::parseInt("4x", v));
    CHECK(!ScenarioLoader::parseInt("12a", v));
    CHECK(!ScenarioLoader::parseInt("3.5", v));
    CHECK(!ScenarioLoader::parseInt("abc", v));
    CHECK(!ScenarioLoader::parseInt("2147483648", v));
    CHECK(!ScenarioLoader::parseInt("99999999999", v));
    CHECK(v == 99);                                                                    // untouched on failure
}

// true if the loader has exactly these error (or warning) line numbers, in any order
static bool sameLines(const ScenarioLoader& ld, bool wantErrors, const int lines[], int count) {
    int have = wantErrors ? ld.errorCount() : ld.warningCount();
    if (have != count) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        bool found = false;
        for (int j = 0; j < have; j++) {
            int line = wantErrors ? ld.error(j).line : ld.warning(j).line;
            if (line == lines[i]) {
                found = true;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

static void testInvalidFiles() {
    World world;
    ScenarioLoader ld;

    // E1 LOCATIONS 6 with 5 records (line 27), E2 ROBOTS 2 with 3 records (line 41)
    CHECK(!ld.load(dataFile("invalid/invalid_01_count_mismatch.txt"), world));
    int e1[2] = {27, 41};
    CHECK(sameLines(ld, true, e1, 2));
    CHECK(ld.warningCount() == 0);

    // E1-E19 except E18, which is a warning (event at 999 > max_steps 50)
    CHECK(!ld.load(dataFile("invalid/invalid_02_bad_values_and_references.txt"), world));
    int e2[18] = {48, 51, 58, 60, 62, 64, 69, 71, 73, 78, 80, 82, 84, 88, 90, 92, 94, 98};
    int w2[1] = {96};
    CHECK(sameLines(ld, true, e2, 18));
    CHECK(sameLines(ld, false, w2, 1));

    // E1 bad PARAM value, E2 VEHICLES, E3 missing field, E4 extra field,
    // E5 ROBOTS without count, E6 text location, E7 no END (line 0).
    // Warnings: PARAM weather, and the three spec PARAMs that are missing.
    CHECK(!ld.load(dataFile("invalid/invalid_03_broken_structure.txt"), world));
    int e3[7] = {27, 32, 38, 44, 47, 52, 0};
    int w3[4] = {29, 0, 0, 0};
    CHECK(sameLines(ld, true, e3, 7));
    CHECK(sameLines(ld, false, w3, 4));

    // printMessages format: errors first, then warnings
    Str outPath = dataFile("../output/test_loader_messages.txt");
    ofstream out(outPath);
    ld.printMessages(out);
    out.close();
    ifstream in(outPath);
    Str line;
    int errorLines = 0;
    int warningLines = 0;
    bool firstIsError = false;
    int n = 0;
    while (getline(in, line)) {
        if (line.compare(0, 12, "error: line ") == 0) {
            errorLines++;
            if (n == 0) {
                firstIsError = true;
            }
        } else if (line.compare(0, 14, "warning: line ") == 0) {
            warningLines++;
        }
        n++;
    }
    in.close();
    CHECK(firstIsError);
    CHECK(errorLines == 7 && warningLines == 4 && n == 11);

    // a file that does not exist
    CHECK(!ld.load(dataFile("no_such_file.txt"), world));
    CHECK(ld.errorCount() == 1);
}

static bool loadsWithCounts(const Str& name, int locs, int conns, int robots, int reqs, int events) {
    World world;
    ScenarioLoader ld;
    bool ok = ld.load(dataFile(name), world);
    if (!ok || ld.warningCount() != 0) {
        cerr << name << ":\n";
        ld.printMessages(cerr);
    }
    bool sorted = true;
    for (int i = 1; i < world.requests.size(); i++) {
        if (world.requests[i - 1].reportTime > world.requests[i].reportTime) {
            sorted = false;
        }
    }
    for (int i = 1; i < world.events.size(); i++) {
        if (world.events[i - 1].time > world.events[i].time) {
            sorted = false;
        }
    }
    for (int i = 1; i < world.robotsById.size(); i++) {
        if (world.robots[world.robotsById[i - 1]].id >= world.robots[world.robotsById[i]].id) {
            sorted = false;
        }
    }
    // every ID map points at the right record after sorting
    bool mapsOk = true;
    for (int i = 0; i < world.requests.size(); i++) {
        int j = NO_INDEX;
        if (!world.requestIndex.find(world.requests[i].id, j) || j != i) {
            mapsOk = false;
        }
    }
    for (int i = 0; i < world.events.size(); i++) {
        const Event& e = world.events[i];
        int id = -1;
        if (e.type == EventType::BLOCK || e.type == EventType::UNBLOCK) {
            id = world.env.connections[e.refIndex].id;
        } else if (e.type == EventType::STATION_DOWN || e.type == EventType::STATION_UP) {
            id = world.env.locations[e.refIndex].id;
        } else if (e.type == EventType::SET_URGENCY) {
            id = world.requests[e.refIndex].id;
        } else {
            id = world.robots[e.refIndex].id;
        }
        if (id != e.refId) {
            mapsOk = false;
        }
    }
    return ok && ld.errorCount() == 0 && ld.warningCount() == 0
        && world.env.locationCount() == locs && world.env.connectionCount() == conns
        && world.robots.size() == robots && world.robotsById.size() == robots
        && world.requests.size() == reqs && world.events.size() == events
        && sorted && mapsOk;
}

static void testValidFiles() {
    CHECK(loadsWithCounts("scenario_provided.txt", 56, 117, 12, 60, 20));
    CHECK(loadsWithCounts("scenario_01_tiny_trace.txt", 8, 11, 3, 4, 1));
    CHECK(loadsWithCounts("scenario_02_cutoff_locations.txt", 12, 18, 4, 6, 3));
    CHECK(loadsWithCounts("scenario_03_ties_and_conflicts.txt", 10, 14, 6, 10, 0));
    CHECK(loadsWithCounts("scenario_04_dynamic_events.txt", 20, 34, 6, 24, 13));
    CHECK(loadsWithCounts("scenario_05_stress.txt", 120, 260, 24, 150, 60));

    // Scenario 04 lists its events out of order: check the stable sort exactly
    World world;
    ScenarioLoader ld;
    CHECK(ld.load(dataFile("scenario_04_dynamic_events.txt"), world));
    int times[13] = {3, 4, 5, 6, 8, 8, 8, 8, 12, 15, 25, 30, 60};
    int refs[13]  = {29, 5, 1, 16, 10, 10, 33, 33, 6, 1, 16, 1, 3};
    EventType types[13] = {EventType::BLOCK, EventType::SET_URGENCY, EventType::ROBOT_OFFLINE,
                           EventType::STATION_DOWN, EventType::BLOCK, EventType::UNBLOCK,
                           EventType::UNBLOCK, EventType::BLOCK, EventType::BLOCK,
                           EventType::UNBLOCK, EventType::STATION_UP, EventType::ROBOT_ONLINE,
                           EventType::SET_URGENCY};
    bool order = world.events.size() == 13;
    for (int i = 0; order && i < 13; i++) {
        const Event& e = world.events[i];
        if (e.time != times[i] || e.refId != refs[i] || e.type != types[i]) {
            order = false;
        }
    }
    CHECK(order);
    CHECK(world.events[1].value == 5);              // SET_URGENCY 5 5
    CHECK(world.params.speed == 2 && world.params.maxSteps == 200);
    CHECK(world.params.agingInterval == 50 && world.params.lowBatteryPercent == 30);

    // loading again into the same World starts from empty
    CHECK(ld.load(dataFile("scenario_01_tiny_trace.txt"), world));
    CHECK(world.env.locationCount() == 8 && world.robots.size() == 3 && world.events.size() == 1);
    CHECK(world.scenarioName == "tiny_trace_01");
}

int main() {
    cout << "test_loader\n";
    testHashMap();
    testTokenizeAndParseInt();
    testInvalidFiles();
    testValidFiles();
    cout << g_checks << " checks, " << g_failed << " failed, " << g_todo << " tests still TODO\n";
    return g_failed == 0 ? 0 : 1;
}
