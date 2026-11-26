// File:       src/output/Logger.h
// Owner:      Farwa
// Purpose:    Writes one line per state change in the agreed format (README
//             "Log format", Report 9.5) to the console and/or a file, and keeps
//             the last lines in memory (step-mode "log" command).
// Depends on: Types
// Used by:    Coordinator, Movement, EventProcessor, Rerouter, Simulation,
//             Statistics, main.cpp
// Milestone:  M2
//
// Every line starts with "t=<time> <KIND>" followed by key=value fields,
// separated by single spaces, e.g.
//     t=4 REROUTE robot=1 on=4 back_to=2 route=2-4-7-5 cost=9
// All numbers written to the log are FILE IDs, not internal indices: callers
// pass IDs (robots[i].id, ...). Routes are pre-formatted by Router::routeToString.
#ifndef RESQGRID_LOGGER_H
#define RESQGRID_LOGGER_H

#include <fstream>
#include "../core/Types.h"

class Logger {
public:
    static const int RECENT_CAPACITY = 200;   // lines kept for the step-mode "log" command

    Logger();
    ~Logger();

    // Open (truncate) `path` for writing; false if it cannot be opened.
    bool openFile(const Str& path);

    void setConsole(bool on) { toConsole = on; }   // --quiet turns the console off
    void setTime(int t)      { currentTime = t; }   // Simulation calls this at the start of each step

    // Core output: "t=<currentTime> " + body, to console (if on), file (if open)
    // and the recent-lines ring buffer. Every helper below calls this.
    void write(const Str& body);

    // --- one helper per line kind (README "Log format") ---
    void assign(int requestId, int robotId, const Str& route, int cost);
    void reject(int requestId, int robotId, int need, int have);              // reason=battery
    void robotState(int robotId, RobotState from, RobotState to, int targetLocId = NO_INDEX);
    void requestState(int requestId, RequestState from, RequestState to);
    void newRequest(int requestId, int locationId, Category cat, int urgency);
    void event(EventType type, int refId, const Str& detail);                 // detail e.g. "(2-5)"
    void reroute(int robotId, int onConnId, int backToLocId, const Str& route, int cost);
    void release(int requestId, int robotId, const Str& reason);              // back to PENDING
    void arrive(int robotId, int locationId);
    void complete(int requestId, int robotId, int response);
    void fail(int requestId, FailReason reason);
    void warning(const Str& message);
    void invalidTransition(const Str& what, int id, const Str& from, const Str& to);

    // --- recent lines: 0 = oldest kept line, recentCount()-1 = newest ---
    int        recentCount() const { return recentSize; }
    const Str& recent(int i) const;

private:
    void remember(const Str& line);           // push into the ring buffer

    ofstream file;
    bool toConsole;
    bool toFile;
    int  currentTime;

    Str  recentLines[RECENT_CAPACITY];        // ring buffer (plain array, no STL container)
    int  recentStart;                         // index of the oldest line
    int  recentSize;                          // number of lines stored (<= RECENT_CAPACITY)
};

#endif // RESQGRID_LOGGER_H
