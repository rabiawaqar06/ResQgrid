// File:       src/main.cpp
// Owner:      Farwa
// Purpose:    Entry point. Parses the command line, loads the scenario, then
//             runs the console simulation (log + final statistics), either all
//             at once or one step at a time (--step). The print helpers below
//             only READ the simulation; all logic lives in modules.
// Depends on: Simulation, Logger, Statistics
// Used by:    the user / evaluator
// Milestone:  M3 (console)
//
// Usage: resqgrid <scenario_file> [--log <path>] [--quiet] [--step]
//   --log <path>  also write the log to <path> (e.g. output/run_log.txt)
//   --quiet       do not print log lines to the console (statistics still print)
//   --step        interactive mode: s, n <k>, env, robots, requests, stats, log [k], run, q
// Exit codes: 0 ok, 1 bad arguments, 2 scenario file invalid, 3 cannot open log file.
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <string>
#include "core/Types.h"
#include "core/Simulation.h"

static void printUsage(const char* prog) {
    cerr << "Usage: " << prog << " <scenario_file> [--log <path>] [--quiet] [--step]\n";
}

static void printHelp() {
    cout << "Commands:\n"
         << "  s          run one step\n"
         << "  n <k>      run k steps\n"
         << "  env        locations, roads and chargers\n"
         << "  robots     robot states\n"
         << "  requests   requests by state\n"
         << "  stats      counts so far (final statistics once finished)\n"
         << "  log [k]    last k log lines (default 20), also works with --quiet\n"
         << "  run        run to the end\n"
         << "  help       this list\n"
         << "  q          quit\n";
}

static void printEnvironment(const World& w) {
    const Environment& env = w.env;
    cout << "--- ENVIRONMENT (network version " << env.networkVersion << ") ---\n";
    cout << "Locations:\n";
    for (int i = 0; i < env.locations.size(); i++) {
        const Location& loc = env.locations[i];
        cout << "  " << loc.id << " " << loc.name << " " << toString(loc.type);
        if (loc.type == LocationType::CHARGING) {
            if (loc.stationOpen) {
                cout << " (charger UP)";
            } else {
                cout << " (charger DOWN)";
            }
        }
        cout << "\n";
    }
    int blocked = 0;
    cout << "Roads:\n";
    for (int i = 0; i < env.connections.size(); i++) {
        const Connection& c = env.connections[i];
        cout << "  " << c.id << " " << env.locations[c.a].id << "-" << env.locations[c.b].id
             << " cost=" << c.cost << " " << toString(c.status) << "\n";
        if (c.status == ConnStatus::BLOCKED) {
            blocked++;
        }
    }
    cout << env.locations.size() << " locations, " << env.connections.size() << " roads ("
         << blocked << " blocked), " << env.chargers.size() << " chargers\n";
}

static void printRobots(const World& w) {
    cout << "--- ROBOTS ---\n";
    for (int k = 0; k < w.robotsById.size(); k++) {
        const Robot& r = w.robots[w.robotsById[k]];
        cout << "  robot " << r.id << " " << toString(r.capability) << " " << toString(r.state)
             << " battery=" << r.battery << "/" << r.maxBattery;
        if (r.onConn == NO_INDEX) {
            cout << " at=" << w.env.locations[r.location].id;
        } else {
            const Connection& c = w.env.connections[r.onConn];
            cout << " on road " << c.id << " (" << w.env.locations[r.location].id << "->"
                 << w.env.locations[c.other(r.location)].id << ", " << r.progress << "/" << c.cost << ")";
        }
        if (r.assignedRequest != NO_INDEX) {
            cout << " request=" << w.requests[r.assignedRequest].id;
        }
        if (r.targetLocation != NO_INDEX) {
            cout << " target=" << w.env.locations[r.targetLocation].id;
        }
        cout << "\n";
    }
}

static void printRequestGroup(const World& w, const Str& title, bool reported, RequestState st) {
    cout << title << ":";
    int shown = 0;
    for (int i = 0; i < w.requests.size(); i++) {
        const Request& q = w.requests[i];
        if (q.reported != reported || (reported && q.state != st)) {
            continue;
        }
        cout << "\n  request " << q.id << " " << toString(q.category) << " urgency=" << q.urgency
             << " at=" << w.env.locations[q.location].id << " reported=" << q.reportTime;
        if (q.assignedRobot != NO_INDEX) {
            cout << " robot=" << w.robots[q.assignedRobot].id;
        }
        if (st == RequestState::IN_PROGRESS) {
            cout << " work_left=" << q.remainingWork;
        }
        if (st == RequestState::PENDING && q.unreachable) {
            cout << " (unreachable)";
        }
        if (st == RequestState::COMPLETED) {
            cout << " done=" << q.completionTime << " response=" << q.responseTime();
        }
        if (st == RequestState::FAILED) {
            cout << " reason=" << toString(q.failReason);
        }
        shown++;
    }
    if (shown == 0) {
        cout << " none";
    }
    cout << "\n";
}

static void printRequests(const World& w, const IndexedMaxHeap& pending) {
    cout << "--- REQUESTS ---\n";
    cout << "Pending queue (" << pending.size() << " in the max-heap, array order, top first):";
    for (int i = 0; i < pending.size(); i++) {
        cout << " " << w.requests[pending.itemAt(i)].id;
    }
    cout << "\n";
    printRequestGroup(w, "PENDING", true, RequestState::PENDING);
    printRequestGroup(w, "ASSIGNED", true, RequestState::ASSIGNED);
    printRequestGroup(w, "IN_PROGRESS", true, RequestState::IN_PROGRESS);
    printRequestGroup(w, "COMPLETED", true, RequestState::COMPLETED);
    printRequestGroup(w, "FAILED", true, RequestState::FAILED);
    printRequestGroup(w, "NOT REPORTED YET", false, RequestState::PENDING);
}

static void printProgress(const Simulation& sim) {
    const World& w = sim.getWorld();
    int counts[5] = { 0, 0, 0, 0, 0 };
    int notReported = 0;
    for (int i = 0; i < w.requests.size(); i++) {
        if (!w.requests[i].reported) {
            notReported++;
        } else {
            counts[(int)w.requests[i].state]++;
        }
    }
    cout << "--- PROGRESS at t=" << w.currentTime << " (max_steps " << w.params.maxSteps << ") ---\n"
         << "pending=" << counts[0] << " assigned=" << counts[1] << " in_progress=" << counts[2]
         << " completed=" << counts[3] << " failed=" << counts[4] << " not_reported=" << notReported
         << " events_left=" << (w.events.size() - w.nextEvent) << "\n";
}

// Runs the interactive loop. Returns when the user quits or input ends.
static void runStepMode(Simulation& sim) {
    printHelp();
    Str line;
    while (true) {
        if (sim.isFinished()) {
            cout << "[finished at t=" << sim.currentTime() << "] > ";
        } else {
            cout << "[next t=" << sim.currentTime() << "] > ";
        }
        if (!getline(cin, line)) {
            cout << "\n";
            return;
        }
        // split into a command word and an optional number
        Str cmd = line;
        Str arg = "";
        size_t space = line.find(' ');
        if (space != Str::npos) {
            cmd = line.substr(0, space);
            arg = line.substr(space + 1);
        }
        if (!cmd.empty() && cmd[cmd.size() - 1] == '\r') {
            cmd.erase(cmd.size() - 1);
        }

        if (cmd == "s" || cmd == "n" || cmd == "run") {
            int k = 1;
            if (cmd == "n") {
                k = atoi(arg.c_str());
                if (k <= 0) {
                    cout << "usage: n <number of steps>\n";
                    continue;
                }
            }
            if (sim.isFinished()) {
                cout << "the simulation is already finished\n";
                continue;
            }
            if (cmd == "run") {
                sim.runAll();
            } else {
                for (int i = 0; i < k && sim.step(); i++) {
                }
            }
            if (sim.isFinished()) {
                cout << "simulation finished\n";
            }
        } else if (cmd == "env") {
            printEnvironment(sim.getWorld());
        } else if (cmd == "robots") {
            printRobots(sim.getWorld());
        } else if (cmd == "requests") {
            printRequests(sim.getWorld(), sim.getPending());
        } else if (cmd == "log") {
            const Logger& log = sim.getLogger();
            int k = 20;
            if (arg != "") {
                k = atoi(arg.c_str());
            }
            int start = log.recentCount() - k;
            if (start < 0) {
                start = 0;
            }
            for (int i = start; i < log.recentCount(); i++) {
                cout << log.recent(i) << "\n";
            }
        } else if (cmd == "stats") {
            printProgress(sim);
            if (sim.isFinished()) {
                sim.getStats().print(cout);
            }
        } else if (cmd == "help") {
            printHelp();
        } else if (cmd == "q" || cmd == "quit") {
            return;
        } else if (cmd != "") {
            cout << "unknown command '" << cmd << "' (type help)\n";
        }
    }
}

int main(int argc, char* argv[]) {
    const char* scenarioPath = nullptr;
    const char* logPath = nullptr;
    bool quiet = false;
    bool stepMode = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else if (strcmp(argv[i], "--step") == 0) {
            stepMode = true;
        } else if (strcmp(argv[i], "--log") == 0) {
            if (i + 1 >= argc) { cerr << "error: --log needs a path\n"; printUsage(argv[0]); return 1; }
            logPath = argv[++i];
        } else if (argv[i][0] == '-') {
            cerr << "error: unknown option " << argv[i] << "\n"; printUsage(argv[0]); return 1;
        } else if (scenarioPath == nullptr) {
            scenarioPath = argv[i];
        } else {
            cerr << "error: more than one scenario file given\n"; printUsage(argv[0]); return 1;
        }
    }
    if (scenarioPath == nullptr) { printUsage(argv[0]); return 1; }

    Simulation sim;
    sim.log().setConsole(!quiet);
    if (logPath != nullptr && !sim.log().openFile(logPath)) {
        cerr << "error: cannot open log file " << logPath << "\n";
        return 3;
    }

    bool loaded = sim.load(scenarioPath);

    if (!loaded) {
        cerr << "Scenario file is invalid: " << sim.getLoader().errorCount() << " error(s)\n";
        sim.getLoader().printMessages(cerr);
        return 2;
    }
    sim.getLoader().printMessages(cerr);  // warnings only, at this point

    const World& w = sim.getWorld();
    cout << "Loaded " << scenarioPath << ": "
         << w.env.locationCount() << " locations, "
         << w.env.connectionCount() << " connections, "
         << w.robots.size() << " robots, "
         << w.requests.size() << " requests, "
         << w.events.size() << " events\n";

    if (stepMode) {
        runStepMode(sim);
    } else {
        sim.runAll();
    }

    // Final statistics only for a finished run (quitting early prints progress instead).
    if (sim.isFinished()) {
        sim.getStats().print(cout);
        sim.getStats().writeToLog(sim.log());
    } else {
        printProgress(sim);
    }
    return 0;
}
