// File:       src/core/EventProcessor.h
// Owner:      Rabia
// Purpose:    Phase 1 of every step: apply every event with time t, in
//             (stable-sorted) file order, directly to the World (Report 4.2,
//             6.3). Redundant events are logged as warnings and have no effect.
// Depends on: World (Environment, Robot, Request), IndexedMaxHeap, Logger
// Used by:    Simulation
// Milestone:  M3
//
// Rerouting does NOT happen here: it runs once after all events (phase 3,
// Rerouter). This class only sets world.networkChangedThisStep.
#ifndef RESQGRID_EVENT_PROCESSOR_H
#define RESQGRID_EVENT_PROCESSOR_H

#include "World.h"
#include "../containers/IndexedMaxHeap.h"
#include "../output/Logger.h"

class EventProcessor {
public:
    EventProcessor(World& world, IndexedMaxHeap& pending, Logger& log);

    // while world.events[world.nextEvent].time <= t: apply it, log
    // it, advance nextEvent. Set world.networkChangedThisStep if any road or
    // station status actually changed. Returns that flag.
    bool applyEventsAt(int t);

    int reassignmentCount() const { return reassignments; }   // requests released by ROBOT_OFFLINE

private:
    // one handler per EventType (Report 6.3):
    //   BLOCK / UNBLOCK      : env.block / env.unblock; warning if redundant.
    //   STATION_DOWN / UP    : env.setStationOpen; robots RECHARGING there stop
    //                          (Rerouter redirects them in phase 3).
    //   SET_URGENCY          : PENDING -> update urgency + pending.changeKey;
    //                          ASSIGNED / IN_PROGRESS -> urgency only (no pre-emption);
    //                          finished -> warning only.
    //   ROBOT_OFFLINE        : robot -> OFFLINE; its request -> PENDING (work kept),
    //                          re-inserted in the heap; counts as a reassignment.
    //   ROBOT_ONLINE         : OFFLINE -> AVAILABLE; warning if it was not offline.
    void applyBlock(const Event& e);
    void applyUnblock(const Event& e);
    void applyStationDown(const Event& e);
    void applyStationUp(const Event& e);
    void applySetUrgency(const Event& e);
    void applyRobotOffline(const Event& e);
    void applyRobotOnline(const Event& e);

    World&          world;
    IndexedMaxHeap& pending;
    Logger&         log;
    int             reassignments = 0;
};

#endif // RESQGRID_EVENT_PROCESSOR_H
