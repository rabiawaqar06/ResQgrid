// File:       src/core/Request.cpp
// Owner:      Farwa
// Purpose:    Request lifecycle state machine and priority key.
// Depends on: Request.h (Types)
// Used by:    see Request.h
// Milestone:  M2
#include "Request.h"

Request::Request(int id_, int reportTime_, int location_, Category cat, int urgency_, int duration_)
    : id(id_), reportTime(reportTime_), location(location_), category(cat),
      urgency(urgency_), duration(duration_), remainingWork(duration_) {}

long long Request::priorityKey(int agingInterval) const {
    return (long long)agingInterval * urgency - reportTime;
}

bool Request::isValidTransition(RequestState from, RequestState to) {
    if (from == RequestState::COMPLETED || from == RequestState::FAILED) {
        return false;
    }
    if (from == RequestState::PENDING) {
        return to == RequestState::ASSIGNED || to == RequestState::FAILED;
    }
    if (from == RequestState::ASSIGNED) {
        return to == RequestState::IN_PROGRESS || to == RequestState::PENDING;
    }
    if (from == RequestState::IN_PROGRESS) {
        return to == RequestState::COMPLETED || to == RequestState::PENDING;
    }
    return false;
}

bool Request::setState(RequestState newState) {
    if (!isValidTransition(state, newState)) {
        return false;
    }
    state = newState;
    return true;
}
