// File:       src/core/Robot.cpp
// Owner:      Farwa
// Purpose:    Robot state machine and capability check.
// Depends on: Robot.h (Types, Queue)
// Used by:    see Robot.h
// Milestone:  M2
#include "Robot.h"

Robot::Robot(int id_, int startLocation, Capability cap, int maxBattery_, int initialBattery)
    : id(id_), capability(cap), maxBattery(maxBattery_), battery(initialBattery),
      location(startLocation) {}

bool Robot::canServe(Category cat) const {
    if (capability == Capability::GENERAL) {
        return true;
    }
    if (capability == Capability::MEDICAL && cat == Category::MEDICAL) return true;
    if (capability == Capability::FIRE && cat == Category::FIRE) return true;
    if (capability == Capability::SUPPLY && cat == Category::SUPPLY) return true;
    if (capability == Capability::SEARCH && cat == Category::SEARCH) return true;
    return false;
}

bool Robot::isValidTransition(RobotState from, RobotState to) {
    if (from == to) {
        return false;
    }
    if (to == RobotState::OFFLINE) {
        return true;
    }
    if (from == RobotState::AVAILABLE) {
        return to == RobotState::TRAVELLING || to == RobotState::RETURNING;
    }
    if (from == RobotState::TRAVELLING) {
        return to == RobotState::PERFORMING || to == RobotState::AVAILABLE || to == RobotState::RETURNING;
    }
    if (from == RobotState::PERFORMING) {
        return to == RobotState::AVAILABLE;
    }
    if (from == RobotState::RETURNING) {
        return to == RobotState::RECHARGING;
    }
    if (from == RobotState::RECHARGING) {
        return to == RobotState::AVAILABLE || to == RobotState::RETURNING;
    }
    if (from == RobotState::OFFLINE) {
        return to == RobotState::AVAILABLE;
    }
    return false;
}

bool Robot::setState(RobotState newState) {
    if (!isValidTransition(state, newState)) {
        return false;
    }
    state = newState;
    return true;
}
