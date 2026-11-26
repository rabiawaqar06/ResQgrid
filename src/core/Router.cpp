// File:       src/core/Router.cpp
// Owner:      Rabia
// Purpose:    Dijkstra over open connections and route reconstruction.
// Depends on: Router.h (Environment, MinHeap, Queue, DynamicArray)
// Used by:    see Router.h
// Milestone:  M2
#include "Router.h"

Router::Router(const Environment& environment) : env(environment) {}

void Router::run(int source) {
    src = source;
    ++runCount;

    int n = env.locationCount();
    dist.assign(n, INF_DIST);
    predLoc.assign(n, NO_INDEX);
    predConn.assign(n, NO_INDEX);
    heap.clear();

    dist[source] = 0;
    DistEntry start;
    start.dist = 0;
    start.node = source;
    heap.push(start);

    DistEntry cur;
    while (heap.popMin(cur)) {
        int u = cur.node;
        int d = cur.dist;
        if (d > dist[u]) {
            continue;   // old heap entry, a shorter path was already found
        }
        const LinkedList<AdjEntry>::Node* p = env.neighbours(u).first();
        while (p != nullptr) {
            int v = p->value.neighbour;
            int c = p->value.conn;
            if (env.isOpen(c)) {
                int newDist = d + env.connections[c].cost;
                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    predLoc[v] = u;
                    predConn[v] = c;
                    DistEntry next;
                    next.dist = newDist;
                    next.node = v;
                    heap.push(next);
                }
            }
            p = p->next;
        }
    }
}

int Router::distanceTo(int loc) const {
    if (loc < 0 || loc >= dist.size()) {
        return INF_DIST;    // no run yet
    }
    return dist[loc];
}

bool Router::reachable(int loc) const {
    return distanceTo(loc) < INF_DIST;
}

bool Router::routeFrom(int start, Queue<int>& out) const {
    out.clear();
    if (!reachable(start)) {
        return false;
    }
    // pred[] points towards the source, so this is already travel order
    int cur = start;
    while (cur != src) {
        out.enqueue(predConn[cur]);
        cur = predLoc[cur];
    }
    return true;
}

bool Router::routeTo(int target, Queue<int>& out) const {
    out.clear();
    if (!reachable(target)) {
        return false;
    }
    // collect target -> source, then enqueue backwards
    DynamicArray<int> back;
    int cur = target;
    while (cur != src) {
        back.push_back(predConn[cur]);
        cur = predLoc[cur];
    }
    for (int i = back.size() - 1; i >= 0; i--) {
        out.enqueue(back[i]);
    }
    return true;
}

int Router::nearestOpenCharger() const {
    // Team decision: if two open chargers are equally near, take the smaller location ID.
    int best = NO_INDEX;
    for (int i = 0; i < env.chargers.size(); i++) {
        int c = env.chargers[i];
        if (!env.isOpenCharger(c) || !reachable(c)) {
            continue;
        }
        if (best == NO_INDEX || dist[c] < dist[best]
            || (dist[c] == dist[best] && env.locations[c].id < env.locations[best].id)) {
            best = c;
        }
    }
    return best;
}

Str Router::routeToString(int startLoc, const Queue<int>& route) const {
    Str s = to_string(env.locations[startLoc].id);
    int cur = startLoc;
    const LinkedList<int>::Node* n = route.first();
    while (n != nullptr) {
        cur = env.connections[n->value].other(cur);
        s += "-" + to_string(env.locations[cur].id);
        n = n->next;
    }
    return s;
}
