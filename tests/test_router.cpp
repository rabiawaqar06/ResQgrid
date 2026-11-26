// File:       tests/test_router.cpp
// Owner:      Rabia
// Purpose:    Tests for MinHeap, Environment (block/unblock) and Router
//             (Dijkstra costs, routes, nearest charger). Own main(), own target.
// Depends on: MinHeap, Environment, Router, ScenarioLoader (to load Scenario 01)
// Used by:    ctest / "before a PR" check
// Milestone:  M1 (MinHeap), M2 (Router)
#include <iostream>
#include "containers/MinHeap.h"
#include "core/Environment.h"
#include "core/Router.h"
#include "core/ScenarioLoader.h"
#include "core/World.h"

static int g_checks = 0, g_failed = 0, g_todo = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; \
    cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)
#define TODO_TEST(name) do { ++g_todo; cout << "  TODO  " << name << "\n"; } while (0)

static void testMinHeap() {
    MinHeap<DistEntry> h;
    DistEntry e;

    // empty heap
    CHECK(h.isEmpty());
    CHECK(h.size() == 0);
    CHECK(!h.popMin(e));
    CHECK(!h.top(e));

    // single element
    e.dist = 4;
    e.node = 1;
    h.push(e);
    CHECK(!h.isEmpty());
    CHECK(h.size() == 1);
    CHECK(h.top(e) && e.dist == 4);
    CHECK(h.popMin(e) && e.dist == 4 && e.node == 1);
    CHECK(h.isEmpty());

    // mixed order with duplicates comes out sorted
    int values[10] = {7, 3, 9, 1, 3, 8, 0, 5, 2, 6};
    for (int i = 0; i < 10; i++) {
        e.dist = values[i];
        e.node = i;
        h.push(e);
    }
    CHECK(h.size() == 10);
    DistEntry t;
    CHECK(h.top(t) && t.dist == 0);

    bool sorted = true;
    bool topMatches = true;
    int last = -1;
    int count = 0;
    while (!h.isEmpty()) {
        h.top(t);
        h.popMin(e);
        if (t.dist != e.dist) {
            topMatches = false;
        }
        if (e.dist < last) {
            sorted = false;
        }
        last = e.dist;
        count++;
    }
    CHECK(sorted);
    CHECK(topMatches);
    CHECK(count == 10);
    CHECK(!h.popMin(e));

    // clear empties the heap and it can be reused
    for (int i = 20; i > 0; i--) {
        e.dist = i;
        h.push(e);
    }
    h.clear();
    CHECK(h.isEmpty());
    CHECK(h.size() == 0);
    e.dist = 3;
    h.push(e);
    e.dist = 1;
    h.push(e);
    CHECK(h.popMin(e) && e.dist == 1);
    CHECK(h.popMin(e) && e.dist == 3);
}

// small map: 10 -(2)- 20 -(3)- 30, location 20 is a charger
static void buildSmallMap(Environment& env) {
    Location loc;
    loc.id = 10;
    loc.type = LocationType::JUNCTION;
    env.addLocation(loc);
    loc.id = 20;
    loc.type = LocationType::CHARGING;
    env.addLocation(loc);
    loc.id = 30;
    loc.type = LocationType::JUNCTION;
    env.addLocation(loc);

    Connection c;
    c.id = 1;
    c.a = 0;
    c.b = 1;
    c.cost = 2;
    env.addConnection(c);
    c.id = 2;
    c.a = 1;
    c.b = 2;
    c.cost = 3;
    env.addConnection(c);
}

// is connection `conn` open, looked at from location `loc`'s adjacency list?
static bool openFrom(const Environment& env, int loc, int conn) {
    const LinkedList<AdjEntry>::Node* p = env.neighbours(loc).first();
    while (p != nullptr) {
        if (p->value.conn == conn) {
            return env.isOpen(p->value.conn);
        }
        p = p->next;
    }
    return false;
}

static void testEnvironmentBlock() {
    Environment env;
    CHECK(env.locationCount() == 0);
    buildSmallMap(env);
    CHECK(env.locationCount() == 3);
    CHECK(env.connectionCount() == 2);
    CHECK(env.chargers.size() == 1 && env.chargers[0] == 1);

    // each road is in both endpoints' lists
    CHECK(env.neighbours(0).size() == 1);
    CHECK(env.neighbours(1).size() == 2);
    CHECK(env.neighbours(2).size() == 1);
    CHECK(env.neighbours(0).first()->value.neighbour == 1);
    CHECK(env.connections[0].other(0) == 1 && env.connections[0].other(1) == 0);

    // blocking one road hides it from both ends
    CHECK(openFrom(env, 0, 0) && openFrom(env, 1, 0));
    CHECK(env.block(0));
    CHECK(!env.isOpen(0));
    CHECK(!openFrom(env, 0, 0));
    CHECK(!openFrom(env, 1, 0));
    CHECK(env.isOpen(1));
    CHECK(!env.block(0));                 // redundant block
    CHECK(env.networkVersion == 0);       // blocking never bumps the version

    // unblock bumps the version only when the road really changed
    CHECK(env.unblock(0));
    CHECK(env.isOpen(0) && openFrom(env, 1, 0));
    CHECK(env.networkVersion == 1);
    CHECK(!env.unblock(0));               // already open
    CHECK(env.networkVersion == 1);

    // repeated block / unblock
    for (int i = 0; i < 3; i++) {
        CHECK(env.block(1));
        CHECK(env.unblock(1));
    }
    CHECK(env.networkVersion == 4);

    // stations: only chargers, redundant changes return false
    CHECK(env.isOpenCharger(1));
    CHECK(!env.isOpenCharger(0));
    CHECK(env.setStationOpen(1, false));
    CHECK(!env.isOpenCharger(1));
    CHECK(!env.setStationOpen(1, false));
    CHECK(env.setStationOpen(1, true));
    CHECK(!env.setStationOpen(0, false)); // not a charger

    env.clear();
    CHECK(env.locationCount() == 0 && env.connectionCount() == 0);
    CHECK(env.chargers.size() == 0 && env.networkVersion == 0);
}

static void testRouterSmall() {
    Environment env;
    buildSmallMap(env);
    Router router(env);
    Queue<int> route;

    // before any run nothing is reachable
    CHECK(router.distanceTo(0) == INF_DIST);
    CHECK(!router.reachable(0));
    CHECK(router.runs() == 0);

    router.run(0);
    CHECK(router.runs() == 1);
    CHECK(router.source() == 0);
    CHECK(router.distanceTo(0) == 0);
    CHECK(router.distanceTo(2) == 5);
    CHECK(router.nearestOpenCharger() == 1);

    // zero-length route is valid
    CHECK(router.routeTo(0, route));
    CHECK(route.isEmpty());
    CHECK(router.routeToString(0, route) == "10");
    CHECK(router.routeFrom(0, route) && route.isEmpty());

    // route both ways over the same run
    CHECK(router.routeTo(2, route) && route.size() == 2);
    CHECK(router.routeToString(0, route) == "10-20-30");
    CHECK(router.routeFrom(2, route) && route.size() == 2);
    CHECK(router.routeToString(2, route) == "30-20-10");

    // unreachable location: blocked road cuts 30 off
    env.block(1);
    router.run(0);
    CHECK(!router.reachable(2));
    CHECK(router.distanceTo(2) == INF_DIST);
    CHECK(!router.routeTo(2, route));
    CHECK(route.isEmpty());
    CHECK(!router.routeFrom(2, route));

    // no open charger reachable
    env.setStationOpen(1, false);
    router.run(0);
    CHECK(router.nearestOpenCharger() == NO_INDEX);
    CHECK(router.runs() == 3);

    // single location map
    Environment one;
    Location loc;
    loc.id = 7;
    one.addLocation(loc);
    Router r1(one);
    r1.run(0);
    CHECK(r1.distanceTo(0) == 0);
    CHECK(r1.routeTo(0, route) && route.isEmpty());
    CHECK(r1.routeToString(0, route) == "7");
}

static int idx(const World& w, int locId) {
    int i = NO_INDEX;
    w.locIndex.find(locId, i);
    return i;
}

static void testScenario01Costs() {
    World world;
    ScenarioLoader loader;
    bool ok = loader.load(Str(RESQGRID_DATA_DIR) + "/scenario_01_tiny_trace.txt", world);
    CHECK(ok);
    if (!ok) {
        loader.printMessages(cerr);
        return;
    }
    Router router(world.env);
    Queue<int> route;

    // 1 -> 5 = 8 via 1-3-2-5 (not the direct 1-2 edge)
    router.run(idx(world, 1));
    CHECK(router.distanceTo(idx(world, 5)) == 8);
    CHECK(router.routeTo(idx(world, 5), route));
    CHECK(router.routeToString(idx(world, 1), route) == "1-3-2-5");

    // same route read with routeFrom after a run from the request
    router.run(idx(world, 5));
    CHECK(router.routeFrom(idx(world, 1), route));
    CHECK(router.routeToString(idx(world, 1), route) == "1-3-2-5");
    CHECK(router.routeFrom(idx(world, 7), route));
    CHECK(router.routeToString(idx(world, 7), route) == "7-5");

    // 7 -> 6 = 6 via 7-5-6 (7-8 is blocked), 7 -> 5 = 4
    router.run(idx(world, 6));
    CHECK(router.routeFrom(idx(world, 7), route));
    CHECK(router.routeToString(idx(world, 7), route) == "7-5-6");
    router.run(idx(world, 7));
    CHECK(router.distanceTo(idx(world, 6)) == 6);
    CHECK(router.distanceTo(idx(world, 5)) == 4);

    // from 8: 8 -> 6 = 3, 8 -> 7 = 9, nearest open charger is 7 via 8-6-5-7
    router.run(idx(world, 8));
    CHECK(router.distanceTo(idx(world, 6)) == 3);
    CHECK(router.distanceTo(idx(world, 7)) == 9);
    CHECK(router.nearestOpenCharger() == idx(world, 7));
    CHECK(router.routeTo(router.nearestOpenCharger(), route));
    CHECK(router.routeToString(idx(world, 8), route) == "8-6-5-7");

    // block connection 4 (2-5): 2 -> 5 = 9 via 2-4-7-5, 5 -> 4 = 6
    int c4 = NO_INDEX;
    CHECK(world.connIndex.find(4, c4));
    CHECK(world.env.block(c4));
    router.run(idx(world, 2));
    CHECK(router.distanceTo(idx(world, 5)) == 9);
    CHECK(router.routeTo(idx(world, 5), route));
    CHECK(router.routeToString(idx(world, 2), route) == "2-4-7-5");
    router.run(idx(world, 5));
    CHECK(router.distanceTo(idx(world, 4)) == 6);

    // the only charger goes down: none left
    world.env.setStationOpen(idx(world, 7), false);
    router.run(idx(world, 8));
    CHECK(router.nearestOpenCharger() == NO_INDEX);
}

int main() {
    cout << "test_router\n";
    testMinHeap();
    testEnvironmentBlock();
    testRouterSmall();
    testScenario01Costs();
    cout << g_checks << " checks, " << g_failed << " failed, " << g_todo << " tests still TODO\n";
    return g_failed == 0 ? 0 : 1;
}
