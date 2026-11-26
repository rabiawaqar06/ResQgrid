// File:       src/core/Environment.h
// Owner:      Rabia
// Purpose:    The disaster map: locations, connections (roads), the adjacency
//             list, charging-station open flags and the network version
//             counter (Report 5.4, 5.9).
// Depends on: DynamicArray, LinkedList, Types
// Used by:    World (World::env), Router, ScenarioLoader, EventProcessor,
//             Rerouter, Movement, main.cpp (step mode)
// Milestone:  M2
//
// Key idea (Report 5.4): each road is ONE Connection record. Both ends get an
// AdjEntry that points to the SAME record, so blocking a road changes one flag
// and both directions see it. Parallel roads are simply two records.
// Everything here uses internal INDICES (0..n-1), never file IDs.
#ifndef RESQGRID_ENVIRONMENT_H
#define RESQGRID_ENVIRONMENT_H

#include "Types.h"
#include "../containers/DynamicArray.h"
#include "../containers/LinkedList.h"

struct Location {
    int          id = 0;                        // ID from the file
    Str          name;
    LocationType type = LocationType::JUNCTION;
    bool         stationOpen = true;            // only meaningful for CHARGING
};

struct Connection {
    int        id = 0;                          // ID from the file
    int        a = NO_INDEX;                    // location index of one end
    int        b = NO_INDEX;                    // location index of the other end
    int        cost = 0;                        // > 0, in cost units
    ConnStatus status = ConnStatus::OPEN;

    // Given one end, return the other end's index.
    int other(int from) const { return from == a ? b : a; }
};

// One entry in a location's adjacency list.
struct AdjEntry {
    int neighbour = NO_INDEX;   // location index at the other end
    int conn = NO_INDEX;        // index into Environment::connections
};

class Environment {
public:
    DynamicArray<Location>             locations;
    DynamicArray<Connection>           connections;
    DynamicArray<LinkedList<AdjEntry>> adjacency;   // adjacency[locIndex]
    DynamicArray<int>                  chargers;    // indices of CHARGING locations
    int networkVersion = 0;                         // +1 on every UNBLOCK (Report 5.9)

    // append the location, append an empty adjacency list for it,
    // and if it is CHARGING add its index to `chargers`. Return its index.
    int addLocation(const Location& loc);

    // append the connection (a, b already converted to indices) and
    // add AdjEntry{b, c} to adjacency[a] and AdjEntry{a, c} to adjacency[b].
    // Return its index.
    int addConnection(const Connection& conn);

    // set status BLOCKED. Return false (no change) if it was already
    // blocked: the caller logs the redundant-event warning.
    bool block(int connIndex);

    // set status OPEN and increment networkVersion. Return false
    // (no change, no version bump) if it was already open.
    bool unblock(int connIndex);

    // status == OPEN.
    bool isOpen(int connIndex) const;

    // set stationOpen on a CHARGING location. Return false if the
    // location is not a charger or the flag already had that value.
    bool setStationOpen(int locIndex, bool open);

    // type == CHARGING and stationOpen.
    bool isOpenCharger(int locIndex) const;

    // empty every array, networkVersion = 0.
    void clear();

    int locationCount() const   { return locations.size(); }
    int connectionCount() const { return connections.size(); }
    const LinkedList<AdjEntry>& neighbours(int locIndex) const { return adjacency[locIndex]; }
};

#endif // RESQGRID_ENVIRONMENT_H
