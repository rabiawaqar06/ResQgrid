// File:       src/output/Statistics.cpp
// Owner:      Farwa
// Purpose:    Computing and printing the final metrics.
// Depends on: Statistics.h (Robot, Request, Logger)
// Used by:    see Statistics.h
// Milestone:  M3
//
// Utilization = 100 * (sum of robot busy steps) / (robots * steps run), a percent.
// The log line leaves out the run time so two runs give identical logs.
#include "Statistics.h"

void Statistics::reset() {
    completed = 0;
    failed = 0;
    failedUnreachable = 0;
    failedNotServed = 0;
    remaining = 0;

    reroutes = 0;
    conflicts = 0;
    reassignments = 0;
    batteryRejections = 0;
    dijkstraRuns = 0;

    totalDistance = 0;
    totalBatteryUsed = 0;

    avgResponse = 0.0;
    maxResponse = 0;
    utilization = 0.0;
    stepsRun = 0;
    runTimeMs = 0.0;
}

void Statistics::addCounters(int reroutes_, int conflicts_, int reassignments_,
                             int batteryRejections_, int dijkstraRuns_) {
    reroutes = reroutes_;
    conflicts = conflicts_;
    reassignments = reassignments_;
    batteryRejections = batteryRejections_;
    dijkstraRuns = dijkstraRuns_;
}

void Statistics::finalize(const DynamicArray<Robot>& robots, const DynamicArray<Request>& requests,
                          int stepsRun_, double runTimeMs_) {
    stepsRun = stepsRun_;
    runTimeMs = runTimeMs_;

    completed = 0;
    failed = 0;
    failedUnreachable = 0;
    failedNotServed = 0;
    remaining = 0;
    maxResponse = 0;
    long long responseSum = 0;

    for (int i = 0; i < requests.size(); i++) {
        const Request& q = requests[i];
        if (q.state == RequestState::COMPLETED) {
            completed++;
            int response = q.responseTime();
            responseSum = responseSum + response;
            if (response > maxResponse) {
                maxResponse = response;
            }
        } else if (q.state == RequestState::FAILED) {
            failed++;
            if (q.failReason == FailReason::UNREACHABLE) {
                failedUnreachable++;
            } else {
                failedNotServed++;
            }
        } else {
            remaining++;
        }
    }

    avgResponse = 0.0;
    if (completed > 0) {
        avgResponse = (double)responseSum / completed;
    }

    totalDistance = 0;
    totalBatteryUsed = 0;
    long long busy = 0;
    for (int i = 0; i < robots.size(); i++) {
        totalDistance = totalDistance + robots[i].distanceTravelled;
        totalBatteryUsed = totalBatteryUsed + robots[i].batteryUsed;
        busy = busy + robots[i].busySteps;
    }

    utilization = 0.0;
    long long possibleSteps = (long long)robots.size() * stepsRun;
    if (possibleSteps > 0) {
        utilization = 100.0 * busy / possibleSteps;
    }
}

void Statistics::print(ostream& out) const {
    out << "========== FINAL STATISTICS ==========" << "\n";
    out << "steps run:            " << stepsRun << "\n";
    out << "completed:            " << completed << "\n";
    out << "failed:               " << failed << "\n";
    out << "  unreachable:        " << failedUnreachable << "\n";
    out << "  not served:         " << failedNotServed << "\n";
    out << "remaining:            " << remaining << "\n";
    out << "average response:     " << avgResponse << "\n";
    out << "max response:         " << maxResponse << "\n";
    out << "robot utilization %:  " << utilization << "\n";
    out << "total distance:       " << totalDistance << "\n";
    out << "total battery used:   " << totalBatteryUsed << "\n";
    out << "conflicts:            " << conflicts << "\n";
    out << "reassignments:        " << reassignments << "\n";
    out << "reroutes:             " << reroutes << "\n";
    out << "battery rejections:   " << batteryRejections << "\n";
    out << "dijkstra runs:        " << dijkstraRuns << "\n";
    out << "execution time (ms):  " << runTimeMs << "\n";
    out << "======================================" << "\n";
}

void Statistics::writeToLog(Logger& log) const {
    log.write("STATS completed=" + to_string(completed)
              + " failed=" + to_string(failed)
              + " unreachable=" + to_string(failedUnreachable)
              + " not_served=" + to_string(failedNotServed)
              + " remaining=" + to_string(remaining)
              + " steps=" + to_string(stepsRun));
    log.write("STATS avg_response=" + to_string(avgResponse)
              + " max_response=" + to_string(maxResponse)
              + " utilization=" + to_string(utilization)
              + " distance=" + to_string(totalDistance)
              + " battery_used=" + to_string(totalBatteryUsed));
    log.write("STATS conflicts=" + to_string(conflicts)
              + " reassignments=" + to_string(reassignments)
              + " reroutes=" + to_string(reroutes)
              + " battery_rejections=" + to_string(batteryRejections)
              + " dijkstra_runs=" + to_string(dijkstraRuns));
}
