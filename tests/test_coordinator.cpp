// File:       tests/test_coordinator.cpp
// Owner:      Sara
// Purpose:    Tests for IndexedMaxHeap and the Coordinator (priority order,
//             tie-breaks, battery rejections), plus a whole-run check on
//             Scenario 01. Own main(), own target.
// Depends on: IndexedMaxHeap, Coordinator, Simulation, ScenarioLoader
// Used by:    ctest / "before a PR" check
// Milestone:  M1 (heap), M3 (Coordinator, Scenario 01 / 03)
//
// HOW THIS FILE IS ORGANISED
//   * testIndexedMaxHeap  needs only DynamicArray (Farwa). It runs for real
//     as soon as DynamicArray works.
//   * testScenario03 / testScenario01 need the loader, router, environment,
//     logger and statistics from Farwa/Rabia. Until those are implemented the
//     world loads empty, so those two tests print SKIP instead of failing.
//     They start checking automatically once the shared modules are real.
#include <iostream>
#include "containers/DynamicArray.h"
#include "containers/IndexedMaxHeap.h"
#include "core/Coordinator.h"
#include "core/Simulation.h"
#include "core/World.h"

static int g_checks = 0, g_failed = 0, g_skipped = 0;

#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; \
    cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)

// Print a skip message once and count it, so the summary explains the gap.
#define SKIP(name) do { ++g_skipped; cout << "  SKIP  " << name << "\n"; } while (0)

// True when the shared containers underneath us are implemented. DynamicArray
// is the only thing the heap itself needs; push/pop/read-back is the smallest
// test that proves it is real (the stubs silently do nothing).
static bool dynamicArrayReady() {
    DynamicArray<int> probe;
    probe.push_back(7);
    probe.push_back(9);
    if (probe.size() != 2 || probe[0] != 7 || probe[1] != 9) {
        return false;
    }
    probe.pop_back();
    return probe.size() == 1 && probe[0] == 7;
}

// --- small helpers to look things up by FILE id (the arrays are index-based) --
static int findRequestIndex(const World& world, int fileId) {
    for (int i = 0; i < world.requests.size(); ++i) {
        if (world.requests[i].id == fileId) {
            return i;
        }
    }
    return -1;
}

// The file id of the robot assigned to a request, or -1 if nobody is assigned.
static int assignedRobotId(const World& world, int requestFileId) {
    int i = findRequestIndex(world, requestFileId);
    if (i < 0) {
        return -1;
    }
    int robotIndex = world.requests[i].assignedRobot;
    if (robotIndex < 0) {
        return -1;
    }
    return world.robots[robotIndex].id;
}

// ---------------------------------------------------------------------------
// IndexedMaxHeap: order, ties, change-key, contains, clear
// ---------------------------------------------------------------------------
static void testIndexedMaxHeap() {
    if (!dynamicArrayReady()) {
        SKIP("IndexedMaxHeap (needs DynamicArray)");
        return;
    }

    IndexedMaxHeap h;
    h.init(10);

    // A fresh heap is empty.
    CHECK(h.isEmpty());
    CHECK(h.size() == 0);
    CHECK(h.peekMax() == -1);
    CHECK(h.extractMax() == -1);
    CHECK(!h.contains(0));

    // Insert out of order. Keys: item4=200 (top), items 2&3=100 (tie), item1=50.
    CHECK(h.insert(3, 100, 3));
    CHECK(h.insert(1, 50, 1));
    CHECK(h.insert(4, 200, 4));
    CHECK(h.insert(2, 100, 2));
    CHECK(h.size() == 4);
    CHECK(h.contains(2));
    CHECK(h.peekMax() == 4);

    // A second insert of the same item must be rejected.
    CHECK(!h.insert(4, 999, 4));

    // expect the items in priority order...
    // 4 first (highest key).
    CHECK(h.extractMax() == 4);
    CHECK(!h.contains(4));
    // ...then the key-100 tie: smaller request id (2) before (3).
    CHECK(h.extractMax() == 2);
    CHECK(h.extractMax() == 3);
    // ...then the remaining low key.
    CHECK(h.extractMax() == 1);
    CHECK(h.isEmpty());

    // changeKey: raise a low item to the top (this is SET_URGENCY).
    h.insert(5, 10, 5);
    h.insert(6, 20, 6);
    CHECK(h.peekMax() == 6);
    CHECK(h.changeKey(5, 500));
    CHECK(h.peekMax() == 5);          // it moved up
    CHECK(h.changeKey(5, 0));         // then push it back down
    CHECK(h.peekMax() == 6);
    CHECK(!h.changeKey(99, 1));       // not in the heap -> false
    CHECK(h.changeKey(5, 999));       // item 5 IS in the heap -> true

    // clear() forgets everyone.
    h.clear();
    CHECK(h.isEmpty());
    CHECK(!h.contains(5));
    CHECK(!h.contains(6));
}

// ---------------------------------------------------------------------------
// Scenario 03: the t=0 assignment order and the robot tie-breaks
// ---------------------------------------------------------------------------
static void testScenario03Assignment() {
    if (!dynamicArrayReady()) {
        SKIP("Scenario 03 assignment (needs DynamicArray)");
        return;
    }

    Simulation sim;
    Str path = Str(RESQGRID_DATA_DIR) + "/scenario_03_ties_and_conflicts.txt";
    if (!sim.load(path) || sim.getWorld().requests.size() == 0) {
        SKIP("Scenario 03 assignment (needs ScenarioLoader/Router)");
        return;
    }

    // Run only the first step: that is where all the t=0 assignments happen.
    sim.step();
    const World& world = sim.getWorld();

    // Expected order from the scenario header.
    CHECK(assignedRobotId(world, 3) == 6);    // Req 3 (urg 5) -> specialist Robot 6
    CHECK(assignedRobotId(world, 1) == 2);    // Req 1 -> Robot 2 (higher battery)
    CHECK(assignedRobotId(world, 2) == 3);    // Req 2 -> Robot 3 (smaller id)
    CHECK(assignedRobotId(world, 11) == 5);   // Req 11 -> the only GENERAL robot
    CHECK(assignedRobotId(world, 4) == 1);    // Req 4 -> Robot 1

    // Requests 12..15 must still be waiting: the GENERAL robot is now busy.
    int waiting = findRequestIndex(world, 12);
    CHECK(waiting >= 0);
    if (waiting >= 0) {
        CHECK(world.requests[waiting].state == RequestState::PENDING);
    }
}

// ---------------------------------------------------------------------------
// Scenario 01: the whole run against the hand trace (Report Appendix A)
// ---------------------------------------------------------------------------
static void testScenario01Run() {
    if (!dynamicArrayReady()) {
        SKIP("Scenario 01 run (needs DynamicArray)");
        return;
    }

    Simulation sim;
    Str path = Str(RESQGRID_DATA_DIR) + "/scenario_01_tiny_trace.txt";
    if (!sim.load(path) || sim.getWorld().requests.size() == 0) {
        SKIP("Scenario 01 run (needs ScenarioLoader/Router)");
        return;
    }

    sim.runAll();
    const World& world = sim.getWorld();

    // The run must end at t=28, not at max_steps (60).
    CHECK(sim.isFinished());
    CHECK(sim.currentTime() == 28);

    // All four requests completed, none failed.
    for (int id = 1; id <= 4; ++id) {
        int i = findRequestIndex(world, id);
        CHECK(i >= 0);
        if (i >= 0) {
            CHECK(world.requests[i].state == RequestState::COMPLETED);
        }
    }

    // Response times (completion - report) from the hand trace: 16, 7, 26, 15.
    int expectedResponse[4] = {16, 7, 26, 15};
    for (int k = 0; k < 4; ++k) {
        int i = findRequestIndex(world, k + 1);
        if (i >= 0) {
            CHECK(world.requests[i].responseTime() == expectedResponse[k]);
        }
    }

    // Counters copied into the statistics at the end of the run.
    const Statistics& stats = sim.getStats();
    CHECK(stats.batteryRejections == 2);
    CHECK(stats.reroutes == 1);
}

int main() {
    cout << "test_coordinator\n";
    testIndexedMaxHeap();
    testScenario03Assignment();
    testScenario01Run();
    cout << g_checks << " checks, " << g_failed << " failed, "
         << g_skipped << " skipped (waiting on shared modules)\n";
    return g_failed == 0 ? 0 : 1;
}
