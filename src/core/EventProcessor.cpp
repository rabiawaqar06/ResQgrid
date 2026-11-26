// File:       src/core/EventProcessor.cpp
// Owner:      Rabia
// Purpose:    Applying dynamic events (phase 1).
// Depends on: EventProcessor.h (World, IndexedMaxHeap, Logger)
// Used by:    Simulation
// Milestone:  M3
#include "EventProcessor.h"

EventProcessor::EventProcessor(World& w, IndexedMaxHeap& p, Logger& l)
    : world(w), pending(p), log(l) {}

bool EventProcessor::applyEventsAt(int t) {
    world.networkChangedThisStep = false;

    // events are sorted by time; "<=" so an event can never get stuck behind the cursor
    while (world.nextEvent < world.events.size() && world.events[world.nextEvent].time <= t) {
        const Event& e = world.events[world.nextEvent];
        if (e.type == EventType::BLOCK) {
            applyBlock(e);
        } else if (e.type == EventType::UNBLOCK) {
            applyUnblock(e);
        } else if (e.type == EventType::STATION_DOWN) {
            applyStationDown(e);
        } else if (e.type == EventType::STATION_UP) {
            applyStationUp(e);
        } else if (e.type == EventType::SET_URGENCY) {
            applySetUrgency(e);
        } else if (e.type == EventType::ROBOT_OFFLINE) {
            applyRobotOffline(e);
        } else {
            applyRobotOnline(e);
        }
        world.nextEvent++;
    }
    return world.networkChangedThisStep;
}

void EventProcessor::applyBlock(const Event& e) {
    const Connection& c = world.env.connections[e.refIndex];
    if (!world.env.block(e.refIndex)) {
        log.warning("BLOCK connection=" + to_string(e.refId) + " ignored: already blocked");
        return;
    }
    Str ends = "(" + to_string(world.env.locations[c.a].id) + "-" + to_string(world.env.locations[c.b].id) + ")";
    log.event(e.type, e.refId, ends);
    world.networkChangedThisStep = true;
}

void EventProcessor::applyUnblock(const Event& e) {
    const Connection& c = world.env.connections[e.refIndex];
    if (!world.env.unblock(e.refIndex)) {
        log.warning("UNBLOCK connection=" + to_string(e.refId) + " ignored: already open");
        return;
    }
    Str ends = "(" + to_string(world.env.locations[c.a].id) + "-" + to_string(world.env.locations[c.b].id) + ")";
    log.event(e.type, e.refId, ends);
    world.networkChangedThisStep = true;
}

void EventProcessor::applyStationDown(const Event& e) {
    // A robot recharging here stops charging (Movement checks the flag) and
    // the Rerouter sends it to another charger in phase 3.
    if (!world.env.setStationOpen(e.refIndex, false)) {
        log.warning("STATION_DOWN location=" + to_string(e.refId) + " ignored: already down");
        return;
    }
    log.event(e.type, e.refId, "");
    world.networkChangedThisStep = true;
}

void EventProcessor::applyStationUp(const Event& e) {
    if (!world.env.setStationOpen(e.refIndex, true)) {
        log.warning("STATION_UP location=" + to_string(e.refId) + " ignored: already open");
        return;
    }
    log.event(e.type, e.refId, "");
    world.networkChangedThisStep = true;
}

void EventProcessor::applySetUrgency(const Event& e) {
    Request& q = world.requests[e.refIndex];
    if (q.isFinished()) {
        log.warning("SET_URGENCY request=" + to_string(e.refId) + " ignored: already " + toString(q.state));
        return;
    }
    q.urgency = e.value;
    log.event(e.type, e.refId, "urgency=" + to_string(e.value));

    // Only a waiting request moves in the heap. ASSIGNED / IN_PROGRESS just
    // keep the new urgency (no pre-emption). A request not reported yet is
    // not in the heap; it enters later with the new urgency.
    if (q.state == RequestState::PENDING && pending.contains(e.refIndex)) {
        pending.changeKey(e.refIndex, q.priorityKey(world.params.agingInterval));
    }
}

void EventProcessor::applyRobotOffline(const Event& e) {
    Robot& r = world.robots[e.refIndex];
    if (r.state == RobotState::OFFLINE) {
        log.warning("ROBOT_OFFLINE robot=" + to_string(e.refId) + " ignored: already offline");
        return;
    }
    log.event(e.type, e.refId, "");

    RobotState old = r.state;
    if (!r.setState(RobotState::OFFLINE)) {
        log.invalidTransition("robot", r.id, toString(old), toString(RobotState::OFFLINE));
        return;
    }
    log.robotState(r.id, old, RobotState::OFFLINE);

    // Team decision: a robot that is partway along a road keeps onConn and
    // progress and just stays there, doing nothing, until ROBOT_ONLINE.
    r.route.clear();
    r.targetLocation = NO_INDEX;

    if (r.assignedRequest == NO_INDEX) {
        return;
    }
    // its request waits again; remainingWork is kept so the next robot continues the work
    int qi = r.assignedRequest;
    Request& q = world.requests[qi];
    RequestState qOld = q.state;
    if (!q.setState(RequestState::PENDING)) {
        log.invalidTransition("request", q.id, toString(qOld), toString(RequestState::PENDING));
        return;
    }
    q.assignedRobot = NO_INDEX;
    r.assignedRequest = NO_INDEX;
    pending.insert(qi, q.priorityKey(world.params.agingInterval), q.id);
    reassignments++;
    log.release(q.id, r.id, "offline");
}

void EventProcessor::applyRobotOnline(const Event& e) {
    Robot& r = world.robots[e.refIndex];
    if (r.state != RobotState::OFFLINE) {
        log.warning("ROBOT_ONLINE robot=" + to_string(e.refId) + " ignored: robot is not offline");
        return;
    }
    log.event(e.type, e.refId, "");
    if (!r.setState(RobotState::AVAILABLE)) {
        log.invalidTransition("robot", r.id, toString(RobotState::OFFLINE), toString(RobotState::AVAILABLE));
        return;
    }
    log.robotState(r.id, RobotState::OFFLINE, RobotState::AVAILABLE);

    // A robot that went offline on a road comes back at the location it last
    // left, so the Coordinator can plan from a real location again.
    if (r.onConn != NO_INDEX) {
        r.onConn = NO_INDEX;
        r.progress = 0;
    }
}
