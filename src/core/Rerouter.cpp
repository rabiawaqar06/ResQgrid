// File:       src/core/Rerouter.cpp
// Owner:      Rabia
// Purpose:    Rerouting robots after road / station changes (phase 3).
// Depends on: Rerouter.h (World, Router, IndexedMaxHeap, Logger)
// Used by:    Simulation
// Milestone:  M3
#include "Rerouter.h"

Rerouter::Rerouter(World& w, Router& r, IndexedMaxHeap& p, Logger& l)
    : world(w), router(r), pending(p), log(l) {}

void Rerouter::rerouteAffected() {
    if (!world.networkChangedThisStep) {
        return;
    }
    for (int k = 0; k < world.robotsById.size(); k++) {
        Robot& r = world.robots[world.robotsById[k]];

        if (r.state == RobotState::RECHARGING) {
            // its station went down: go to another open charger
            if (chargerDown(r) && !replan(r)) {
                log.warning("robot=" + to_string(r.id) + " station is down and no other charger can be reached");
            }
            continue;
        }
        if (!r.isMoving()) {
            continue;
        }

        // a RETURNING robot that was stopped earlier, standing short of its charger
        bool stuck = r.state == RobotState::RETURNING && r.onConn == NO_INDEX
                     && r.route.isEmpty() && r.location != r.targetLocation;
        if (!routeBlocked(r) && !chargerDown(r) && !stuck) {
            continue;
        }

        bool turned = false;
        if (r.onConn != NO_INDEX && !world.env.isOpen(r.onConn)) {
            turnBack(r);
            turned = true;
        }
        if (replan(r)) {
            continue;
        }
        if (r.state == RobotState::TRAVELLING) {
            releaseRequest(r);
        } else {
            // RETURNING with no reachable charger: wait (old route, or stand
            // still if it turned back) and try again at the next change
            if (turned) {
                r.route.clear();
            }
            log.warning("robot=" + to_string(r.id) + " has no route to an open charger, waiting");
        }
    }
}

bool Rerouter::routeBlocked(const Robot& r) const {
    if (r.onConn != NO_INDEX && !world.env.isOpen(r.onConn)) {
        return true;
    }
    // scan the rest of the planned route
    const LinkedList<int>::Node* p = r.route.first();
    while (p != nullptr) {
        if (!world.env.isOpen(p->value)) {
            return true;
        }
        p = p->next;
    }
    return false;
}

bool Rerouter::chargerDown(const Robot& r) const {
    if (r.state == RobotState::RECHARGING) {
        return !world.env.isOpenCharger(r.location);
    }
    if (r.state == RobotState::RETURNING && r.targetLocation != NO_INDEX) {
        return !world.env.isOpenCharger(r.targetLocation);
    }
    return false;
}

void Rerouter::turnBack(Robot& r) {
    if (r.progress == 0) {
        r.onConn = NO_INDEX;    // it had not left the location yet
        return;
    }
    // Turn around on the closed road: now it walks from the far end back to
    // r.location. Walking back takes real steps and battery, which Movement
    // charges as it moves (Scenario 01: Request 1 completes at t=16).
    const Connection& c = world.env.connections[r.onConn];
    int back = r.location;
    r.location = c.other(back);
    r.progress = c.cost - r.progress;
}

bool Rerouter::replan(Robot& r) {
    // Plan from the location the robot reaches next. If it is on a road,
    // the rest of that road (extra units) is added to the battery check.
    int from = r.location;
    int extra = 0;
    bool walkingBack = false;
    if (r.onConn != NO_INDEX) {
        const Connection& c = world.env.connections[r.onConn];
        from = c.other(r.location);
        extra = c.cost - r.progress;
        walkingBack = !world.env.isOpen(r.onConn);
    }
    int bc = world.params.batteryPerCostUnit;
    int cost = 0;

    if (r.state == RobotState::TRAVELLING) {
        if (r.assignedRequest == NO_INDEX) {
            return false;
        }
        const Request& q = world.requests[r.assignedRequest];
        // one run from the request gives the robot distance and the charger reserve
        router.run(q.location);
        if (!router.reachable(from)) {
            return false;
        }
        cost = router.distanceTo(from);
        int reserve = 0;
        int charger = router.nearestOpenCharger();
        if (charger != NO_INDEX) {
            reserve = router.distanceTo(charger);
        }
        int need = bc * (extra + cost + reserve) + world.params.batteryPerWorkStep * q.remainingWork;
        if (r.battery < need) {
            return false;
        }
        router.routeFrom(from, r.route);
    } else {
        // RETURNING or RECHARGING: go to the nearest open charger
        router.run(from);
        int charger = router.nearestOpenCharger();
        if (charger == NO_INDEX) {
            return false;
        }
        cost = router.distanceTo(charger);
        if (r.battery < bc * (extra + cost)) {
            return false;
        }
        router.routeTo(charger, r.route);
        r.targetLocation = charger;
        if (r.state == RobotState::RECHARGING) {
            if (!r.setState(RobotState::RETURNING)) {
                log.invalidTransition("robot", r.id, toString(RobotState::RECHARGING), toString(RobotState::RETURNING));
                return false;
            }
            log.robotState(r.id, RobotState::RECHARGING, RobotState::RETURNING, world.env.locations[charger].id);
        }
    }

    reroutes++;
    int onId = NO_INDEX;
    int backId = NO_INDEX;
    if (walkingBack) {
        onId = world.env.connections[r.onConn].id;
        backId = world.env.locations[from].id;
    }
    log.reroute(r.id, onId, backId, router.routeToString(from, r.route), cost);
    return true;
}

void Rerouter::releaseRequest(Robot& r) {
    // If still on a road, finish it at once so the robot stands at a real
    // location again (battery and distance are charged for those units).
    if (r.onConn != NO_INDEX) {
        const Connection& c = world.env.connections[r.onConn];
        int units = c.cost - r.progress;
        int used = world.params.batteryPerCostUnit * units;
        if (used > r.battery) {
            used = r.battery;   // battery never below 0
        }
        r.battery -= used;
        r.batteryUsed += used;
        r.distanceTravelled += units;
        r.location = c.other(r.location);
        r.onConn = NO_INDEX;
        r.progress = 0;
    }
    r.route.clear();
    r.targetLocation = NO_INDEX;

    if (r.assignedRequest != NO_INDEX) {
        int qi = r.assignedRequest;
        Request& q = world.requests[qi];
        RequestState old = q.state;
        if (q.setState(RequestState::PENDING)) {
            q.assignedRobot = NO_INDEX;
            r.assignedRequest = NO_INDEX;
            pending.insert(qi, q.priorityKey(world.params.agingInterval), q.id);
            reassignments++;
            log.release(q.id, r.id, "no_route");
        } else {
            log.invalidTransition("request", q.id, toString(old), toString(RequestState::PENDING));
        }
    }

    // low battery: head for a charger if one can be reached, otherwise just wait
    bool low = r.battery * 100 < world.params.lowBatteryPercent * r.maxBattery;
    if (low) {
        router.run(r.location);
        int charger = router.nearestOpenCharger();
        if (charger != NO_INDEX
            && r.battery >= world.params.batteryPerCostUnit * router.distanceTo(charger)
            && router.routeTo(charger, r.route)) {
            RobotState old = r.state;
            if (r.setState(RobotState::RETURNING)) {
                r.targetLocation = charger;
                log.robotState(r.id, old, RobotState::RETURNING, world.env.locations[charger].id);
                return;
            }
        }
        r.route.clear();
    }
    RobotState old = r.state;
    if (r.setState(RobotState::AVAILABLE)) {
        log.robotState(r.id, old, RobotState::AVAILABLE);
    } else {
        log.invalidTransition("robot", r.id, toString(old), toString(RobotState::AVAILABLE));
    }
}
