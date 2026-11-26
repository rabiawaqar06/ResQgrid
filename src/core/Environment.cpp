// File:       src/core/Environment.cpp
// Owner:      Rabia
// Purpose:    Building the map and changing road / station status.
// Depends on: Environment.h (DynamicArray, LinkedList, Types)
// Used by:    see Environment.h
// Milestone:  M2
#include "Environment.h"

int Environment::addLocation(const Location& loc) {
    locations.push_back(loc);
    adjacency.push_back(LinkedList<AdjEntry>());
    int index = locations.size() - 1;
    if (loc.type == LocationType::CHARGING) {
        chargers.push_back(index);
    }
    return index;
}

int Environment::addConnection(const Connection& conn) {
    connections.push_back(conn);
    int c = connections.size() - 1;

    // undirected road: add it at both ends, both pointing to the same record
    AdjEntry fromA;
    fromA.neighbour = conn.b;
    fromA.conn = c;
    adjacency[conn.a].pushBack(fromA);

    AdjEntry fromB;
    fromB.neighbour = conn.a;
    fromB.conn = c;
    adjacency[conn.b].pushBack(fromB);
    return c;
}

bool Environment::block(int connIndex) {
    if (connections[connIndex].status == ConnStatus::BLOCKED) {
        return false;
    }
    connections[connIndex].status = ConnStatus::BLOCKED;
    return true;
}

bool Environment::unblock(int connIndex) {
    if (connections[connIndex].status == ConnStatus::OPEN) {
        return false;
    }
    connections[connIndex].status = ConnStatus::OPEN;
    networkVersion++;
    return true;
}

bool Environment::isOpen(int connIndex) const {
    return connections[connIndex].status == ConnStatus::OPEN;
}

bool Environment::setStationOpen(int locIndex, bool open) {
    if (locations[locIndex].type != LocationType::CHARGING) {
        return false;
    }
    if (locations[locIndex].stationOpen == open) {
        return false;
    }
    locations[locIndex].stationOpen = open;
    return true;
}

bool Environment::isOpenCharger(int locIndex) const {
    return locations[locIndex].type == LocationType::CHARGING && locations[locIndex].stationOpen;
}

void Environment::clear() {
    locations.clear();
    connections.clear();
    adjacency.clear();
    chargers.clear();
    networkVersion = 0;
}
