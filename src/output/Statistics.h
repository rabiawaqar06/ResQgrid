// File:       src/output/Statistics.h
// Owner:      Farwa
// Purpose:    Final metrics of a run (spec section 8, Report Appendix A):
//             completed / failed / remaining, reroutes, conflicts,
//             reassignments, battery rejections, distance, battery used,
//             response times, utilization, run time.
// Depends on: Robot, Request, Logger, DynamicArray
// Used by:    Simulation, main.cpp
// Milestone:  M3
//
// Counters that only another module can see (conflicts, rejections, reroutes,
// reassignments, Dijkstra runs) are kept by that module and copied in with
// addCounters() at the end, so Statistics does not depend on them.
// Per-robot distance / battery / busy steps and per-request completion times
// are read from the robots and requests in finalize().
#ifndef RESQGRID_STATISTICS_H
#define RESQGRID_STATISTICS_H

#include <iostream>
#include "../core/Types.h"
#include "../core/Robot.h"
#include "../core/Request.h"
#include "../containers/DynamicArray.h"
#include "Logger.h"

class Statistics {
public:
    // --- request outcomes ---
    int completed = 0;
    int failed = 0;
    int failedUnreachable = 0;
    int failedNotServed = 0;
    int remaining = 0;              // not finished when the run stopped (0 after failing leftovers)

    // --- coordination counters (copied in by Simulation) ---
    int reroutes = 0;
    int conflicts = 0;
    int reassignments = 0;
    int batteryRejections = 0;
    int dijkstraRuns = 0;

    // --- movement / battery ---
    long long totalDistance = 0;
    long long totalBatteryUsed = 0;

    // --- derived ---
    double avgResponse = 0.0;
    int    maxResponse = 0;
    double utilization = 0.0;       // percent: 100 * busy steps / (robots * steps)
    int    stepsRun = 0;
    double runTimeMs = 0.0;

    // Set every field back to 0 (two runs in one process give the same numbers).
    void reset();

    void addCounters(int reroutes, int conflicts, int reassignments, int batteryRejections, int dijkstraRuns);

    // Count outcomes from `requests`, response times
    // (completionTime - reportTime, Report Appendix A) for COMPLETED ones,
    // sum distance / battery / busy steps from `robots`, compute averages.
    void finalize(const DynamicArray<Robot>& robots, const DynamicArray<Request>& requests,
                  int stepsRun, double runTimeMs);

    // Readable block of all metrics (console / final screen).
    void print(ostream& out) const;

    // The same metrics as "STATS key=value" log lines (run time left out).
    void writeToLog(Logger& log) const;
};

#endif // RESQGRID_STATISTICS_H
