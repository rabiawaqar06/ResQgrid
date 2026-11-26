// File:       src/core/Rerouter.h
// Owner:      Rabia
// Purpose:    Phase 3: after ALL events of the step, find the moving robots
//             affected by a road or station change and reroute them, or
//             release their request (Report 4.7, 7.5).
// Depends on: World (Environment, Robot, Request), Router, IndexedMaxHeap, Logger
// Used by:    Simulation
// Milestone:  M3
//
// Policy (Report 4.7), for each TRAVELLING or RETURNING robot:
//   * on the blocked road  -> turn back to the location it came from (battery
//                              is used for the distance back), reroute from there;
//   * blocked road later in the route -> reroute from the next location;
//   * no feasible route    -> request back to PENDING (a reassignment), robot
//                              AVAILABLE, or RETURNING if its battery is low.
// A charging station going down is handled the same way for robots heading to
// it or recharging there. Detection is a scan of every moving robot's route,
// O(R * L) (Report 7.5).
#ifndef RESQGRID_REROUTER_H
#define RESQGRID_REROUTER_H

#include "World.h"
#include "Router.h"
#include "../containers/IndexedMaxHeap.h"
#include "../output/Logger.h"

class Rerouter {
public:
    Rerouter(World& world, Router& router, IndexedMaxHeap& pending, Logger& log);

    // if !world.networkChangedThisStep return; otherwise check
    // every robot in ID order (world.robotsById) and apply the policy above.
    void rerouteAffected();

    int rerouteCount() const      { return reroutes; }
    int reassignmentCount() const { return reassignments; }

private:
    // true if the robot's current road or any road left in its route is BLOCKED.
    bool routeBlocked(const Robot& r) const;

    // true if the robot is heading to / recharging at a charger that is now down.
    bool chargerDown(const Robot& r) const;

    // robot on a blocked road turns around on it and walks back to `location`
    // (progress counted from the far end). Movement then moves it back, so the
    // walk back costs real steps and battery (hand trace: Request 1 done at t=16).
    void turnBack(Robot& r);

    // new route from the robot's (next) location to its target
    // (request location, or nearest open charger when RETURNING); battery
    // feasibility as in Report 4.5. False if there is no feasible route.
    bool replan(Robot& r);

    // request -> PENDING (keep remainingWork), clear both links,
    // pending.insert, ++reassignments, robot -> AVAILABLE or RETURNING.
    void releaseRequest(Robot& r);

    World&          world;
    Router&         router;
    IndexedMaxHeap& pending;
    Logger&         log;
    int reroutes = 0;
    int reassignments = 0;
};

#endif // RESQGRID_REROUTER_H
