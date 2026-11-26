// File:       src/core/Robot.h
// Owner:      Farwa
// Purpose:    One rescue robot: identity, capability, battery, state machine,
//             assigned request, planned route and position on a road
//             (Report 4.1, 4.4, 4.5).
// Depends on: Types, Queue
// Used by:    World, ScenarioLoader, Coordinator, Movement, EventProcessor,
//             Rerouter, Statistics, main.cpp (step mode)
// Milestone:  M2
//
// Position (Report 4.1): if onConn == NO_INDEX the robot is AT `location`.
// Otherwise it is on connection `onConn`, has left `location`, and has
// travelled `progress` units of that road towards the other end.
// The robot stores INDICES (location, request), never file IDs.
#ifndef RESQGRID_ROBOT_H
#define RESQGRID_ROBOT_H

#include "Types.h"
#include "../containers/Queue.h"

class Robot {
public:
    // --- identity and limits (from the ROBOTS section) ---
    int        id = 0;
    Capability capability = Capability::GENERAL;
    int        maxBattery = 0;

    // --- dynamic state ---
    int        battery = 0;
    RobotState state = RobotState::AVAILABLE;
    int        location = NO_INDEX;         // current or last location index
    int        onConn = NO_INDEX;           // connection index while on a road
    int        progress = 0;                // units travelled along onConn
    int        assignedRequest = NO_INDEX;  // request index (set/cleared together with Request::assignedRobot)
    int        targetLocation = NO_INDEX;   // where the route ends (request or charger)
    Queue<int> route;                       // remaining connection indices, in travel order

    // --- counters for Statistics (Report 8 / spec section 8) ---
    int distanceTravelled = 0;
    int batteryUsed = 0;
    int busySteps = 0;                      // steps not AVAILABLE/OFFLINE (utilization)

    Robot() = default;
    Robot(int id, int startLocation, Capability cap, int maxBattery, int initialBattery);

    // True if capability is GENERAL or matches the category
    // (MEDICAL serves MEDICAL, ...). Report 6.2 (candidate rule).
    bool canServe(Category cat) const;

    // The allowed robot transitions from the Report 4.4 figure:
    //   AVAILABLE->TRAVELLING, TRAVELLING->PERFORMING, PERFORMING->AVAILABLE,
    //   AVAILABLE->RETURNING, RETURNING->RECHARGING, RECHARGING->AVAILABLE,
    //   any other state->OFFLINE, OFFLINE->AVAILABLE,
    //   TRAVELLING->AVAILABLE / TRAVELLING->RETURNING (lost every route, 4.7),
    //   RECHARGING->RETURNING (station went down, 4.7).
    // Everything else (including X->X) is false.
    static bool isValidTransition(RobotState from, RobotState to);

    // If isValidTransition(state, newState) set it and return true;
    // otherwise leave the state unchanged and return false. The CALLER logs the
    // rejection with Logger::invalidTransition (Robot does not depend on Logger).
    bool setState(RobotState newState);

    bool isOnRoad() const { return onConn != NO_INDEX; }
    bool isMoving() const { return state == RobotState::TRAVELLING || state == RobotState::RETURNING; }
};

#endif // RESQGRID_ROBOT_H
