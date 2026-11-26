// File:       src/core/Movement.h
// Owner:      Sara
// Purpose:    Phase 5 (low-battery check) and phase 6 (robot actions): move
//             `speed` units along the route (crossing locations within one
//             step), work, recharge; battery updates (Report 4.1, 4.2, 4.5).
// Depends on: World (Robot, Request, Environment), Router, Logger
// Used by:    Simulation
// Milestone:  M3
//
// Rules (all from the report):
//   * Robots act in increasing robot-ID order (world.robotsById).
//   * Move: one cost unit at a time, up to `speed` units per step. Each unit
//     costs batteryPerCostUnit. Reaching a location mid-step, the robot takes
//     the next road of its route in the same step.
//   * A robot that reaches its request location starts work in the NEXT step.
//     Each work step: remainingWork - 1, battery - batteryPerWorkStep.
//     remainingWork == 0 -> request COMPLETED (completionTime = t), robot AVAILABLE.
//   * A robot that reaches its charger starts recharging in the NEXT step:
//     + rechargePerStep, up to maxBattery; AVAILABLE as soon as it is full.
//   * Battery never goes below 0 and a BLOCKED road is never entered: in both
//     cases the robot waits and a warning is logged (should not happen if the
//     Coordinator and Rerouter did their job).
//
// Position (Robot.h): onConn == NO_INDEX -> at `location`. Otherwise on road
// onConn, `progress` units away from `location` (the end it came from).
#ifndef RESQGRID_MOVEMENT_H
#define RESQGRID_MOVEMENT_H

#include "World.h"
#include "Router.h"
#include "../output/Logger.h"

class Movement {
public:
    Movement(World& world, Router& router, Logger& log);

    // Phase 5: every robot still AVAILABLE with battery below
    // lowBatteryPercent% of its maximum goes RETURNING to the nearest open
    // charger. If no charger can be reached it simply stays AVAILABLE.
    void lowBatteryCheck();

    // Phase 6: each robot, in ID order, moves / works / recharges depending on
    // the state it had at the START of this phase.
    void actAll(int t);

    // How many requests were completed in the last actAll() call.
    int completedThisStep() const { return completedNow; }

private:
    void move(Robot& r, int t);       // TRAVELLING or RETURNING
    void work(Robot& r, int t);       // PERFORMING
    void recharge(Robot& r);          // RECHARGING

    // Called when the route is finished: TRAVELLING -> PERFORMING (request
    // ASSIGNED -> IN_PROGRESS) or RETURNING -> RECHARGING.
    void arrive(Robot& r, int t);

    // battery * 100 < lowBatteryPercent * maxBattery (whole numbers only).
    bool isLowBattery(const Robot& r) const;

    // Change a state, log it (STATE line) or log INVALID if not allowed.
    bool changeRobotState(Robot& r, RobotState newState, int targetLocId = NO_INDEX);
    bool changeRequestState(Request& q, RequestState newState);

    World&  world;
    Router& router;
    Logger& log;
    int     completedNow = 0;
};

#endif // RESQGRID_MOVEMENT_H
