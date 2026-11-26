// File:       src/core/Simulation.cpp
// Owner:      Sara
// Purpose:    The 7-phase step loop and termination rule.
// Depends on: Simulation.h (all core modules, Logger, Statistics), <chrono>
// Used by:    main.cpp, tools/check_invariants
// Milestone:  M3
#include "Simulation.h"
#include <chrono>

Simulation::Simulation()
    : router(world.env),
      coord(world, router, logger),
      events(world, coord.pending(), logger),
      rerouter(world, router, coord.pending(), logger),
      movement(world, router, logger) {}

bool Simulation::load(const Str& path) {
    stats.reset();
    runTimeMs = 0.0;

    loaded = loader.load(path, world);
    if (!loaded) {
        finished = true;            // nothing to run
        return false;
    }

    // Start of the run: time 0, nothing reported, no event applied yet.
    world.currentTime = 0;
    world.nextRequest = 0;
    world.nextEvent = 0;
    world.networkChangedThisStep = false;

    coord.init();                   // heap sized for every request
    finished = false;
    return true;
}

bool Simulation::step() {
    if (finished) {
        return false;
    }

    // Measure only the time spent simulating (not waiting for step-mode input).
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    int t = world.currentTime;
    logger.setTime(t);

    // 1. Events of this step, in file order.
    events.applyEventsAt(t);

    // 2. New requests of this step become PENDING, in file order.
    releaseNewRequests(t);

    // 3. Reroute, once, after ALL events (so block + unblock in one step does nothing).
    if (world.networkChangedThisStep) {
        rerouter.rerouteAffected();
    }

    // 4. Assignment (Algorithm 2).
    coord.assignStep();

    // 5. Idle robots with low battery go to a charger.
    movement.lowBatteryCheck();

    // 6. Robots act in ID order: move, work or recharge.
    movement.actAll(t);

    // 7. Record: every module above already wrote its log lines.

    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    runTimeMs += chrono::duration<double, milli>(end - start).count();

    // Termination check (Report 4.8). The step that meets it is the last one.
    if (t >= world.params.maxSteps || allDone()) {
        finish();                   // currentTime stays at the last step run
    } else {
        world.currentTime = t + 1;
    }
    return true;
}

void Simulation::runAll() {
    while (step()) {
        // each call runs one step; step() returns false once finished
    }
}

void Simulation::releaseNewRequests(int t) {
    // Requests are sorted by report time, so the new ones are at the cursor.
    while (world.nextRequest < world.requests.size()
           && world.requests[world.nextRequest].reportTime <= t) {
        int index = world.nextRequest;
        Request& q = world.requests[index];

        q.reported = true;              // its state is already PENDING (starting state)
        q.remainingWork = q.duration;

        logger.newRequest(q.id, world.env.locations[q.location].id, q.category, q.urgency);
        coord.addPending(index);

        world.nextRequest++;
    }
}

bool Simulation::allDone() const {
    if (world.nextRequest < world.requests.size()) {
        return false;               // some requests are still to be reported
    }
    if (world.nextEvent < world.events.size()) {
        return false;               // some events are still to come
    }
    for (int i = 0; i < world.requests.size(); ++i) {
        if (!world.requests[i].isFinished()) {
            return false;
        }
    }
    return true;
}

void Simulation::finish() {
    // Every request that is not COMPLETED becomes FAILED (Report 4.3).
    for (int i = 0; i < world.requests.size(); ++i) {
        Request& q = world.requests[i];
        if (q.isFinished()) {
            continue;
        }

        // Reason: no route existed -> unreachable; otherwise no robot was free in time.
        if (q.unreachable) {
            q.failReason = FailReason::UNREACHABLE;
        } else {
            q.failReason = FailReason::NOT_SERVED;
        }

        // A request still with a robot is released first (ASSIGNED / IN_PROGRESS
        // -> PENDING), because the state machine only allows PENDING -> FAILED.
        if (q.assignedRobot != NO_INDEX) {
            world.robots[q.assignedRobot].assignedRequest = NO_INDEX;
            q.assignedRobot = NO_INDEX;
            q.setState(RequestState::PENDING);
        }
        // (A request never reported is still PENDING, its starting state.)

        RequestState old = q.state;
        if (q.setState(RequestState::FAILED)) {
            logger.fail(q.id, q.failReason);
        } else {
            logger.invalidTransition("request", q.id, toString(old), toString(RequestState::FAILED));
        }
    }

    // Copy the counters each module kept, then compute the final metrics.
    int reassignments = rerouter.reassignmentCount() + events.reassignmentCount();
    stats.addCounters(rerouter.rerouteCount(), coord.conflictCount(), reassignments,
                      coord.batteryRejectionCount(), router.runs());

    int stepsRun = world.currentTime + 1;   // steps 0 .. currentTime
    stats.finalize(world.robots, world.requests, stepsRun, runTimeMs);

    finished = true;
}
