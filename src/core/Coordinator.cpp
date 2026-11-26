// File:       src/core/Coordinator.cpp
// Owner:      Sara
// Purpose:    Assignment phase (Algorithm 2) and the pending heap.
// Depends on: Coordinator.h (World, IndexedMaxHeap, Router, Logger)
// Used by:    Simulation
// Milestone:  M3
#include "Coordinator.h"

Coordinator::Coordinator(World& w, Router& r, Logger& l) : world(w), router(r), log(l) {}

void Coordinator::init() {
    pendingHeap.init(world.requests.size());
    for (int c = 0; c < NUM_CAPABILITIES; ++c) {
        freeCount[c] = 0;
    }
    conflicts = 0;
    batteryRejections = 0;
}

void Coordinator::addPending(int requestIndex) {
    const Request& req = world.requests[requestIndex];
    long long key = req.priorityKey(world.params.agingInterval);
    pendingHeap.insert(requestIndex, key, req.id);
}

void Coordinator::assignStep() {
    countFreeRobots();

    // Requests we could not serve this step; they go back into the heap at the end.
    DynamicArray<int> deferred;

    while (!pendingHeap.isEmpty() && anyRobotFree()) {
        int reqIndex = pendingHeap.extractMax();
        if (reqIndex == NO_INDEX) {
            break;      // safety: heap said not empty but returned nothing
        }
        Request& req = world.requests[reqIndex];

        // Safety: only PENDING requests belong in the heap. Drop anything else.
        if (req.state != RequestState::PENDING) {
            continue;
        }

        // Skip without running Dijkstra (Algorithm 2, line 5).
        bool networkUnchanged = req.unreachable && req.unreachableVersion == world.networkVersion();
        if (!someoneCanServe(req.category) || networkUnchanged) {
            deferred.push_back(reqIndex);
            continue;
        }

        // ONE Dijkstra from the request: distances to every robot and to the chargers.
        router.run(req.location);

        bool anyCapableReachable = false;
        int best = chooseRobot(reqIndex, anyCapableReachable);

        if (best != NO_INDEX && assign(reqIndex, best)) {
            req.unreachable = false;
        } else {
            if (best == NO_INDEX && !anyCapableReachable) {
                // No capable robot can reach it: retry only after the network changes.
                req.unreachable = true;
                req.unreachableVersion = world.networkVersion();
            }
            deferred.push_back(reqIndex);
        }
    }

    // Put deferred requests back. Their key did not change, so their order is kept.
    for (int i = 0; i < deferred.size(); ++i) {
        addPending(deferred[i]);
    }
}

void Coordinator::countFreeRobots() {
    for (int c = 0; c < NUM_CAPABILITIES; ++c) {
        freeCount[c] = 0;
    }
    for (int i = 0; i < world.robots.size(); ++i) {
        const Robot& robot = world.robots[i];
        if (robot.state == RobotState::AVAILABLE) {
            freeCount[(int)robot.capability]++;
        }
    }
}

bool Coordinator::anyRobotFree() const {
    for (int c = 0; c < NUM_CAPABILITIES; ++c) {
        if (freeCount[c] > 0) {
            return true;
        }
    }
    return false;
}

bool Coordinator::someoneCanServe(Category cat) const {
    // Category values MEDICAL..SEARCH have the same numbers as the matching Capability values.
    return freeCount[(int)cat] > 0 || freeCount[(int)Capability::GENERAL] > 0;
}

int Coordinator::batteryNeeded(int distToRequest, int distToCharger, int duration) const {
    return world.params.batteryPerCostUnit * (distToRequest + distToCharger)
         + world.params.batteryPerWorkStep * duration;
}

int Coordinator::chooseRobot(int requestIndex, bool& anyCapableReachable) {
    const Request& req = world.requests[requestIndex];
    anyCapableReachable = false;

    // Battery reserve: distance from the request to the nearest open charger.
    // The router's source is the request, so its distances are exactly what we need.
    int distToCharger = 0;
    int charger = router.nearestOpenCharger();
    if (charger != NO_INDEX) {
        distToCharger = router.distanceTo(charger);
    } else {
        log.warning("no open charger reachable from request=" + to_string(req.id)
                    + " battery reserve=0");
    }

    int best = NO_INDEX;
    int bestDist = 0;
    int candidates = 0;

    // Robots in ID order, so REJECT lines always come out in the same order.
    for (int k = 0; k < world.robotsById.size(); ++k) {
        int i = world.robotsById[k];
        const Robot& robot = world.robots[i];

        if (!robot.canServe(req.category)) {
            continue;
        }
        int dist = router.distanceTo(robot.location);
        if (dist >= INF_DIST) {
            continue;                       // no route between robot and request
        }
        anyCapableReachable = true;         // counts busy robots too

        if (robot.state != RobotState::AVAILABLE) {
            continue;
        }

        int need = batteryNeeded(dist, distToCharger, req.duration);
        if (robot.battery < need) {
            ++batteryRejections;
            log.reject(req.id, robot.id, need, robot.battery);
            continue;
        }

        // Robot i is a candidate.
        ++candidates;
        if (best == NO_INDEX || better(i, dist, best, bestDist)) {
            best = i;
            bestDist = dist;
        }
    }

    if (candidates > 1) {
        ++conflicts;
    }
    return best;
}

bool Coordinator::better(int robotA, int distA, int robotB, int distB) const {
    const Robot& a = world.robots[robotA];
    const Robot& b = world.robots[robotB];

    // 1. smaller travel cost
    if (distA != distB) {
        return distA < distB;
    }
    // 2. specialist before GENERAL
    bool aSpecialist = (a.capability != Capability::GENERAL);
    bool bSpecialist = (b.capability != Capability::GENERAL);
    if (aSpecialist != bSpecialist) {
        return aSpecialist;
    }
    // 3. higher battery
    if (a.battery != b.battery) {
        return a.battery > b.battery;
    }
    // 4. smaller robot ID
    return a.id < b.id;
}

bool Coordinator::assign(int requestIndex, int robotIndex) {
    Request& req = world.requests[requestIndex];
    Robot& robot = world.robots[robotIndex];

    // Check both transitions first, so we never change only one side.
    if (!Request::isValidTransition(req.state, RequestState::ASSIGNED)) {
        log.invalidTransition("request", req.id, toString(req.state), toString(RequestState::ASSIGNED));
        return false;
    }
    if (!Robot::isValidTransition(robot.state, RobotState::TRAVELLING)) {
        log.invalidTransition("robot", robot.id, toString(robot.state), toString(RobotState::TRAVELLING));
        return false;
    }

    // Route from the robot to the request, read from the predecessor array.
    if (!router.routeFrom(robot.location, robot.route)) {
        return false;
    }
    int cost = router.distanceTo(robot.location);

    req.setState(RequestState::ASSIGNED);
    robot.setState(RobotState::TRAVELLING);

    // Both links are set together, so they can never disagree (Report 4.3).
    req.assignedRobot = robotIndex;
    robot.assignedRequest = requestIndex;
    robot.targetLocation = req.location;

    freeCount[(int)robot.capability]--;

    // One ASSIGN line records both state changes (request ASSIGNED, robot TRAVELLING).
    log.assign(req.id, robot.id, router.routeToString(robot.location, robot.route), cost);
    return true;
}
