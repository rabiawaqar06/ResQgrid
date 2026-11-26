// File:       src/core/Router.h
// Owner:      Rabia
// Purpose:    Dijkstra with lazy deletion from one source over OPEN roads,
//             giving dist[] and pred[] for every location, routes read from
//             pred[], and the nearest open charger (Report 6.1, Algorithm 1).
// Depends on: Environment, MinHeap, Queue, DynamicArray, Types
// Used by:    Coordinator (one run per request, FROM THE REQUEST), Rerouter,
//             Movement (nearest charger / low battery)
// Milestone:  M2
//
// Roads are undirected, so a run from the REQUEST location gives the distance
// to every robot AND to the nearest charger in one pass (Report 6.1). The
// robot's route is then read by following pred[] from the robot to the source:
// use routeFrom(). When the run started at the robot (going to a charger,
// rerouting), use routeTo(target), which reverses the pred chain.
//
// Route = queue of CONNECTION indices in travel order. Cost of a route equals
// dist[] of its far end. A route of length 0 (start == source) is valid.
#ifndef RESQGRID_ROUTER_H
#define RESQGRID_ROUTER_H

#include "Types.h"
#include "Environment.h"
#include "../containers/DynamicArray.h"
#include "../containers/MinHeap.h"
#include "../containers/Queue.h"

// One MinHeap entry. Smaller distance first.
struct DistEntry {
    int dist = 0;
    int node = NO_INDEX;
    bool operator<(const DistEntry& other) const { return dist < other.dist; }
};

class Router {
public:
    explicit Router(const Environment& env);

    // Algorithm 1. dist[v] = INF_DIST, predLoc[v] = predConn[v] = -1
    // for all v; dist[s] = 0; lazy-deletion loop; relax only OPEN connections.
    // Increments runCount (for the performance experiments, Report 12.2).
    void run(int source);

    int source() const { return src; }

    // dist[loc] from the last run (INF_DIST if unreachable or no run yet).
    int distanceTo(int loc) const;

    // distanceTo(loc) < INF_DIST.
    bool reachable(int loc) const;

    // fill `out` with the route start -> source by following
    // predConn[] from `start` (use after a run FROM THE REQUEST). Clears `out`
    // first. Returns false if start is unreachable.
    bool routeFrom(int start, Queue<int>& out) const;

    // fill `out` with the route source -> target (use after a run
    // FROM THE ROBOT). Follow pred[] back from target, then reverse (no STL:
    // e.g. collect into a DynamicArray and enqueue backwards). False if unreachable.
    bool routeTo(int target, Queue<int>& out) const;

    // index of the OPEN charger with the smallest dist from the last
    // run's source; NO_INDEX if none is reachable. Ties (team decision): the
    // smaller location ID wins.
    int nearestOpenCharger() const;

    // location IDs along the route joined with '-', e.g. "2-4-7-5",
    // starting at startLoc (for the log). Empty route -> just the start ID.
    Str routeToString(int startLoc, const Queue<int>& route) const;

    int runs() const { return runCount; }

private:
    const Environment& env;
    DynamicArray<int>  dist;       // dist[v]
    DynamicArray<int>  predLoc;    // previous location on the best path to v
    DynamicArray<int>  predConn;   // connection used to reach v
    MinHeap<DistEntry> heap;       // reused by every run
    int src = NO_INDEX;
    int runCount = 0;
};

#endif // RESQGRID_ROUTER_H
