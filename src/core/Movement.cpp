// File:       src/core/Movement.cpp
// Owner:      Sara
// Purpose:    Robot actions (move, work, recharge) and the low-battery rule.
// Depends on: Movement.h (World, Router, Logger)
// Used by:    Simulation
// Milestone:  M3
#include "Movement.h"

Movement::Movement(World& w, Router& r, Logger& l) : world(w), router(r), log(l) {}

// ---------------------------------------------------------------------------
// Phase 5: low-battery check
// ---------------------------------------------------------------------------
void Movement::lowBatteryCheck() {
    for (int k = 0; k < world.robotsById.size(); ++k) {
        Robot& r = world.robots[world.robotsById[k]];

        if (r.state != RobotState::AVAILABLE || !isLowBattery(r)) {
            continue;
        }

        // Dijkstra from the robot, then the closest open charger.
        router.run(r.location);
        int charger = router.nearestOpenCharger();
        if (charger == NO_INDEX) {
            // No charger reachable: stay AVAILABLE. (No warning here, it would
            // repeat every step; the Coordinator's battery rule keeps it idle.)
            continue;
        }

        // Not enough battery to even get there: it cannot go (Report 11:
        // "otherwise unavailable"). The battery rule stops it from taking jobs.
        int distance = router.distanceTo(charger);
        if (r.battery < distance * world.params.batteryPerCostUnit) {
            continue;
        }

        if (!router.routeTo(charger, r.route)) {
            continue;
        }
        if (changeRobotState(r, RobotState::RETURNING, world.env.locations[charger].id)) {
            r.targetLocation = charger;
        }
        // If the robot is already AT the charger the route is empty; actAll()
        // makes it arrive this step and it recharges from the next step.
    }
}

// ---------------------------------------------------------------------------
// Phase 6: actions
// ---------------------------------------------------------------------------
void Movement::actAll(int t) {
    completedNow = 0;

    for (int k = 0; k < world.robotsById.size(); ++k) {
        Robot& r = world.robots[world.robotsById[k]];

        // Utilization: every step the robot is doing something.
        if (r.state != RobotState::AVAILABLE && r.state != RobotState::OFFLINE) {
            r.busySteps++;
        }

        // Decide on the state at the start of the phase. A robot that arrives
        // during move() does not also work / recharge in the same step.
        if (r.state == RobotState::TRAVELLING || r.state == RobotState::RETURNING) {
            move(r, t);
        } else if (r.state == RobotState::PERFORMING) {
            work(r, t);
        } else if (r.state == RobotState::RECHARGING) {
            recharge(r);
        }
        // AVAILABLE and OFFLINE robots do nothing.
    }
}

void Movement::move(Robot& r, int t) {
    int costPerUnit = world.params.batteryPerCostUnit;

    // Move one unit at a time, at most `speed` units.
    for (int unit = 0; unit < world.params.speed; ++unit) {

        // At a location: take the next road of the route.
        if (r.onConn == NO_INDEX) {
            int nextConn;
            if (!r.route.front(nextConn)) {
                break;                              // route finished
            }
            if (!world.env.isOpen(nextConn)) {
                log.warning("robot=" + to_string(r.id) + " waits: next road "
                            + to_string(world.env.connections[nextConn].id) + " is blocked");
                break;
            }
            r.route.dequeue(nextConn);
            r.onConn = nextConn;
            r.progress = 0;
        }

        // Battery for one unit.
        if (r.battery < costPerUnit) {
            log.warning("robot=" + to_string(r.id) + " stopped: battery too low to move");
            break;
        }
        r.battery -= costPerUnit;
        r.batteryUsed += costPerUnit;
        r.distanceTravelled += 1;
        r.progress += 1;

        // End of the road reached: now standing at the other end.
        const Connection& road = world.env.connections[r.onConn];
        if (r.progress >= road.cost) {
            r.location = road.other(r.location);
            r.onConn = NO_INDEX;
            r.progress = 0;
        }
    }

    // Standing at a location with nothing left in the route: arrived.
    // (Also true straight away for a zero-length route.)
    if (r.onConn == NO_INDEX && r.route.isEmpty()) {
        arrive(r, t);
    }
}

void Movement::arrive(Robot& r, int /* t: not needed, the logger has the time */) {
    log.arrive(r.id, world.env.locations[r.location].id);

    if (r.state == RobotState::TRAVELLING) {
        if (r.assignedRequest == NO_INDEX) {
            // Should not happen: travelling with no job. Become free again.
            changeRobotState(r, RobotState::AVAILABLE);
            return;
        }
        Request& q = world.requests[r.assignedRequest];
        changeRobotState(r, RobotState::PERFORMING);
        changeRequestState(q, RequestState::IN_PROGRESS);
        // Work starts in the next step.
    } else if (r.state == RobotState::RETURNING) {
        if (!world.env.isOpenCharger(r.location)) {
            // Station went down; the Rerouter redirects it next time the
            // network changes. Just report it.
            log.warning("robot=" + to_string(r.id) + " reached a charger that is not open");
            return;
        }
        changeRobotState(r, RobotState::RECHARGING);
        // Recharging starts in the next step.
    }
}

void Movement::work(Robot& r, int t) {
    if (r.assignedRequest == NO_INDEX) {
        changeRobotState(r, RobotState::AVAILABLE);
        return;
    }
    Request& q = world.requests[r.assignedRequest];

    int costPerStep = world.params.batteryPerWorkStep;
    if (r.battery < costPerStep) {
        log.warning("robot=" + to_string(r.id) + " cannot work: battery too low");
        return;
    }
    r.battery -= costPerStep;
    r.batteryUsed += costPerStep;
    q.remainingWork -= 1;

    if (q.remainingWork > 0) {
        return;
    }

    // Job done.
    if (changeRequestState(q, RequestState::COMPLETED)) {
        q.completionTime = t;
        log.complete(q.id, r.id, q.responseTime());
        completedNow++;
    }
    // Clear both links together (Report 4.3).
    q.assignedRobot = NO_INDEX;
    r.assignedRequest = NO_INDEX;
    r.targetLocation = NO_INDEX;
    changeRobotState(r, RobotState::AVAILABLE);
}

void Movement::recharge(Robot& r) {
    if (!world.env.isOpenCharger(r.location)) {
        // Station is down: no charging. The Rerouter moves the robot elsewhere.
        return;
    }
    r.battery += world.params.rechargePerStep;
    if (r.battery >= r.maxBattery) {
        r.battery = r.maxBattery;
        r.targetLocation = NO_INDEX;
        changeRobotState(r, RobotState::AVAILABLE);   // a candidate again next step
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool Movement::isLowBattery(const Robot& r) const {
    return r.battery * 100 < world.params.lowBatteryPercent * r.maxBattery;
}

bool Movement::changeRobotState(Robot& r, RobotState newState, int targetLocId) {
    RobotState old = r.state;
    if (!r.setState(newState)) {
        log.invalidTransition("robot", r.id, toString(old), toString(newState));
        return false;
    }
    log.robotState(r.id, old, newState, targetLocId);
    return true;
}

bool Movement::changeRequestState(Request& q, RequestState newState) {
    RequestState old = q.state;
    if (!q.setState(newState)) {
        log.invalidTransition("request", q.id, toString(old), toString(newState));
        return false;
    }
    log.requestState(q.id, old, newState);
    return true;
}
