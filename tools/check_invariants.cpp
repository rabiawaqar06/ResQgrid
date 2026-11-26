// File:       tools/check_invariants.cpp
// Owner:      Rabia
// Purpose:    Invariant checker. Runs the real Simulation step by step and,
//             after every step, checks:
//               1. every robot has 0 <= battery <= max_battery;
//               2. no robot enters a BLOCKED road or keeps going forward on one
//                  (turning back towards the location it came from is allowed);
//               3. no request is held by two robots, and robot/request links agree;
//               4. a request becomes COMPLETED at most once and never changes after.
//             With a log file it also checks that every request has at most one
//             COMPLETE line and counts INVALID lines (there should be none).
// Usage:      check_invariants <scenario_file> [log_file]
// Exit code:  0 if every check passed, 1 otherwise, 2 if the scenario is invalid.
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include "core/Simulation.h"

static int g_errors = 0;

static void report(int t, const Str& text) {
    if (g_errors < 20) {
        cout << "  VIOLATION t=" << t << ": " << text << "\n";
    }
    g_errors++;
}

static void checkLog(const char* path, const World& w) {
    ifstream in(path);
    if (!in.is_open()) {
        cout << "  cannot open log " << path << "\n";
        g_errors++;
        return;
    }
    DynamicArray<int> completeCount;
    completeCount.assign(w.requests.size(), 0);
    int invalid = 0;
    int lines = 0;
    Str line;
    while (getline(in, line)) {
        lines++;
        if (line.find(" INVALID ") != Str::npos) {
            invalid++;
        }
        size_t p = line.find(" COMPLETE request=");
        if (p != Str::npos) {
            int id = atoi(line.c_str() + p + 18);
            int index = NO_INDEX;
            if (!w.requestIndex.find(id, index)) {
                report(-1, "COMPLETE for unknown request " + to_string(id));
            } else {
                completeCount[index]++;
                if (completeCount[index] > 1) {
                    report(-1, "request " + to_string(id) + " completed twice in the log");
                }
            }
        }
    }
    if (invalid > 0) {
        report(-1, to_string(invalid) + " INVALID line(s) in the log");
    }
    cout << "  log: " << lines << " lines, " << invalid << " INVALID lines\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <scenario_file> [log_file]\n";
        return 1;
    }
    Simulation sim;
    sim.log().setConsole(false);
    if (!sim.load(argv[1])) {
        cerr << argv[1] << ": scenario is invalid\n";
        return 2;
    }
    const World& w = sim.getWorld();
    int n = w.robots.size();

    DynamicArray<int> prevOn;        // road each robot was on after the last step
    DynamicArray<int> prevLoc;       // location it had left
    DynamicArray<int> heading;       // forced direction while walking back on a blocked road
    DynamicArray<int> doneCount;     // times each request became COMPLETED
    DynamicArray<int> prevState;
    for (int i = 0; i < n; i++) {
        prevOn.push_back(w.robots[i].onConn);
        prevLoc.push_back(w.robots[i].location);
        heading.push_back(NO_INDEX);
    }
    for (int q = 0; q < w.requests.size(); q++) {
        doneCount.push_back(0);
        prevState.push_back((int)w.requests[q].state);
    }

    int steps = 0;
    int walkBacks = 0;
    while (true) {
        int t = sim.currentTime();
        if (!sim.step()) {
            break;
        }
        steps++;

        for (int i = 0; i < n; i++) {
            const Robot& r = w.robots[i];
            int oldHeading = heading[i];
            if (r.battery < 0 || r.battery > r.maxBattery) {
                report(t, "robot " + to_string(r.id) + " battery " + to_string(r.battery));
            }
            if (r.assignedRequest != NO_INDEX && w.requests[r.assignedRequest].assignedRobot != i) {
                report(t, "robot " + to_string(r.id) + " and its request are not linked both ways");
            }

            int c = r.onConn;
            if (c != NO_INDEX && !w.env.isOpen(c)) {
                int head = w.env.connections[c].other(r.location);
                if (heading[i] == NO_INDEX) {
                    if (prevOn[i] != c) {
                        report(t, "robot " + to_string(r.id) + " entered blocked road " + to_string(w.env.connections[c].id));
                    } else if (head != prevLoc[i]) {
                        report(t, "robot " + to_string(r.id) + " kept going forward on blocked road " + to_string(w.env.connections[c].id));
                    } else {
                        heading[i] = head;
                        walkBacks++;
                    }
                } else if (head != heading[i]) {
                    report(t, "robot " + to_string(r.id) + " turned around again on a blocked road");
                }
            } else {
                heading[i] = NO_INDEX;
            }
            // crossed a blocked road to its far end within one step
            if (c == NO_INDEX && prevOn[i] != NO_INDEX && !w.env.isOpen(prevOn[i]) && oldHeading == NO_INDEX
                && r.location == w.env.connections[prevOn[i]].other(prevLoc[i])) {
                report(t, "robot " + to_string(r.id) + " crossed blocked road " + to_string(w.env.connections[prevOn[i]].id));
            }
            prevOn[i] = r.onConn;
            prevLoc[i] = r.location;
        }

        for (int q = 0; q < w.requests.size(); q++) {
            const Request& req = w.requests[q];
            int holders = 0;
            for (int i = 0; i < n; i++) {
                if (w.robots[i].assignedRequest == q) {
                    holders++;
                }
            }
            if (holders > 1) {
                report(t, "request " + to_string(req.id) + " held by " + to_string(holders) + " robots");
            }
            if (prevState[q] == (int)RequestState::COMPLETED && req.state != RequestState::COMPLETED) {
                report(t, "request " + to_string(req.id) + " changed after COMPLETED");
            }
            if (prevState[q] != (int)RequestState::COMPLETED && req.state == RequestState::COMPLETED) {
                doneCount[q]++;
                if (doneCount[q] > 1) {
                    report(t, "request " + to_string(req.id) + " completed twice");
                }
            }
            prevState[q] = (int)req.state;
        }
    }

    cout << argv[1] << ": " << steps << " steps, " << walkBacks << " walk-back(s) on blocked roads\n";
    if (argc >= 3) {
        checkLog(argv[2], w);
    }
    if (g_errors == 0) {
        cout << "  ALL INVARIANTS HOLD\n";
        return 0;
    }
    cout << "  " << g_errors << " VIOLATION(S)\n";
    return 1;
}
