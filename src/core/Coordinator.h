// File:       src/core/Coordinator.h
// Owner:      Sara
// Purpose:    Owns the pending-request priority queue and runs the assignment
//             phase (Report 6.2, Algorithm 2): picks the best robot for each
//             request in priority order, logs battery rejections, counts
//             conflicts.
// Depends on: World (Robot, Request, Environment), IndexedMaxHeap, Router, Logger
// Used by:    Simulation (EventProcessor / Rerouter use pending() through it)
// Milestone:  M3 (started in M2)
//
// Candidate (Report 6.2): AVAILABLE, canServe(category), reachable, and passes
// the battery feasibility rule (Report 4.5):
//     battery >= b_c * (d(robot, req) + d(req, nearest open charger)) + b_w * duration
//     (no open charger reachable -> reserve term is 0, warning logged; Report 11)
// Winner among candidates, in order:
//     1. smallest travel cost   2. specialist before GENERAL
//     3. higher battery         4. smaller robot ID
// A request with more than one candidate counts as one CONFLICT.
#ifndef RESQGRID_COORDINATOR_H
#define RESQGRID_COORDINATOR_H

#include "World.h"
#include "Router.h"
#include "../containers/DynamicArray.h"
#include "../containers/IndexedMaxHeap.h"
#include "../output/Logger.h"

class Coordinator {
public:
    Coordinator(World& world, Router& router, Logger& log);

    // Prepares the heap for every request index and resets the counters.
    // Call once after loading.
    void init();

    // Phase 2 helper: puts request `requestIndex` into the heap with
    // key = A * urgency - reportTime and its ID as the tie-breaker.
    void addPending(int requestIndex);

    // Phase 4, Algorithm 2 (Report 6.2):
    //   count free robots; deferred = empty
    //   while heap not empty and some robot AVAILABLE:
    //     r = extractMax
    //     if no free robot can serve r.category, or (r unreachable and network
    //        version unchanged since then): defer, continue
    //     Dijkstra ONCE from r.location
    //     best = chooseRobot(r)   (logs each battery rejection)
    //     if best: assign r to best
    //     else: mark r unreachable if no capable robot had a route; defer
    //   put every deferred request back into the heap
    // No head-of-line blocking: an unservable request never stops the loop.
    void assignStep();

    IndexedMaxHeap&       pending()       { return pendingHeap; }
    const IndexedMaxHeap& pending() const { return pendingHeap; }

    int conflictCount() const         { return conflicts; }
    int batteryRejectionCount() const { return batteryRejections; }

private:
    // freeCount[cap] = number of AVAILABLE robots with that capability (Report 5.9).
    void countFreeRobots();

    // True if at least one robot (any capability) is AVAILABLE.
    bool anyRobotFree() const;

    // True if some AVAILABLE robot has this capability or is GENERAL. O(1).
    bool someoneCanServe(Category cat) const;

    // Battery needed for a job, feasibility rule of Report 4.5.
    int batteryNeeded(int distToRequest, int distToCharger, int duration) const;

    // Uses the Dijkstra result of the last router.run(request location).
    // Applies the candidate filter, logs each REJECT, counts a conflict when
    // there is more than one candidate. Returns the winning robot index or
    // NO_INDEX. anyCapableReachable = some capable robot (in any state) can
    // reach the request; if false the request is marked unreachable.
    int chooseRobot(int requestIndex, bool& anyCapableReachable);

    // True if robot a beats robot b (tie-break rules 1-4), given their distances.
    bool better(int robotA, int distA, int robotB, int distB) const;

    // Links request and robot, request -> ASSIGNED, robot -> TRAVELLING, fills
    // the robot's route from the predecessor array, logs ASSIGN. A zero-length
    // route still goes through TRAVELLING; work starts next step (Report 4.1).
    // Returns false (nothing changed) if a state transition is not allowed.
    bool assign(int requestIndex, int robotIndex);

    World&         world;
    Router&        router;
    Logger&        log;
    IndexedMaxHeap pendingHeap;
    int freeCount[NUM_CAPABILITIES] = {0, 0, 0, 0, 0};
    int conflicts = 0;
    int batteryRejections = 0;
};

#endif // RESQGRID_COORDINATOR_H
