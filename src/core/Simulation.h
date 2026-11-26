// File:       src/core/Simulation.h
// Owner:      Sara
// Purpose:    The simulation engine: loads a scenario, runs the 7-phase step
//             loop, applies the termination rule, and gives main.cpp read-only
//             access to everything (Report 4.2, 4.8).
// Depends on: World, ScenarioLoader, Router, Coordinator, Movement,
//             EventProcessor, Rerouter, Logger, Statistics
// Used by:    main.cpp (full run and step mode), tools/check_invariants
// Milestone:  M3
//
// One step at time t (Report 4.2), in exactly this order:
//   1. Events        EventProcessor::applyEventsAt(t)
//   2. New requests  reportTime == t -> PENDING, Coordinator::addPending (file order)
//   3. Reroute       Rerouter::rerouteAffected()  (only if the network changed)
//   4. Assignment    Coordinator::assignStep()
//   5. Low battery   Movement::lowBatteryCheck()
//   6. Action        Movement::actAll(t)          (robots in ID order)
//   7. Record        every module already wrote its log lines; nothing extra
// Termination (Report 4.8): stop after the first step t where t == maxSteps,
// or where every request is COMPLETED/FAILED, no request is still to be
// reported and no event is left. Then every unfinished request becomes
// FAILED with a reason (Report 4.3) and the statistics are computed.
#ifndef RESQGRID_SIMULATION_H
#define RESQGRID_SIMULATION_H

#include "World.h"
#include "ScenarioLoader.h"
#include "Router.h"
#include "Coordinator.h"
#include "Movement.h"
#include "EventProcessor.h"
#include "Rerouter.h"
#include "../output/Logger.h"
#include "../output/Statistics.h"

class Simulation {
public:
    Simulation();

    // Loads and validates the file. Returns false if it had errors (read them
    // with getLoader()); a failed load leaves the simulation finished, so
    // step() does nothing. Load once per Simulation object.
    bool load(const Str& path);

    // Runs ONE full step (phases 1-7) at the current time, then checks the
    // termination rule. Returns true if a step was run, false if the
    // simulation was already finished. Step mode calls this once per "s".
    bool step();

    // Calls step() until the simulation is finished (console mode).
    void runAll();

    bool isFinished() const { return finished; }
    bool isLoaded() const   { return loaded; }

    // --- read-only access for main.cpp (step mode) and tools ---
    int                   currentTime() const { return world.currentTime; }
    const World&          getWorld() const    { return world; }
    const ScenarioLoader& getLoader() const   { return loader; }
    const Statistics&     getStats() const    { return stats; }
    const Logger&         getLogger() const   { return logger; }
    const IndexedMaxHeap& getPending() const  { return coord.pending(); }

    // Logger setup before running (main.cpp: --log, --quiet).
    Logger& log() { return logger; }

private:
    // Phase 2: every request with reportTime == t becomes visible (PENDING)
    // and goes into the heap, in file order.
    void releaseNewRequests(int t);

    // Termination rule 2: all requests finished, none left to report, no events left.
    bool allDone() const;

    // End of run: fail unfinished requests, compute the statistics.
    void finish();

    // Member order matters: world and logger must be constructed before the
    // modules that keep references to them.
    World          world;
    Logger         logger;
    ScenarioLoader loader;
    Statistics     stats;
    Router         router;
    Coordinator    coord;
    EventProcessor events;
    Rerouter       rerouter;
    Movement       movement;
    bool           loaded = false;
    bool           finished = false;
    double         runTimeMs = 0.0;   // sum of the time spent inside step()
};

#endif // RESQGRID_SIMULATION_H
