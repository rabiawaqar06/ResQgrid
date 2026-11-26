// File:       src/core/World.h
// Owner:      Shared (written together on Day 1, committed by Rabia)
// Purpose:    The single shared state of a run. The loader fills it; every
//             simulation module reads and writes it, so a change made by one
//             part (a road blocked) is seen at once by the others (Report 3).
// Depends on: Types, Environment, Robot, Request, DynamicArray, HashMap
// Used by:    ScenarioLoader, EventProcessor, Rerouter, Coordinator, Movement,
//             Simulation, main.cpp (step mode)
// Milestone:  M0 (struct agreed) - it holds data only, no logic.
//
// Conventions
//   * Internally everything is an INDEX (0..n-1) into the arrays below.
//     File IDs are only used when reading the file and when writing the log.
//     Convert with the *Index maps (ID -> index) and back with array[i].id.
//   * Locations, connections, adjacency, charger flags and the network version
//     live in `env` (Environment.h) so Router only needs the map.
//   * `requests` and `events` are STABLE-SORTED by time at load time
//     (Report 5.7); the cursors point at the first one not yet processed.
#ifndef RESQGRID_WORLD_H
#define RESQGRID_WORLD_H

#include "Types.h"
#include "Environment.h"
#include "Robot.h"
#include "Request.h"
#include "../containers/DynamicArray.h"
#include "../containers/HashMap.h"

// PARAM lines (Report 9.3). The five spec parameters appear in every provided
// scenario file; the last two are our optional extensions with defaults.
struct Params {
    int maxSteps = 0;                 // max_steps: last time step (termination rule 1)
    int speed = 1;                    // speed: cost units travelled per step
    int batteryPerCostUnit = 1;       // battery_per_cost_unit
    int batteryPerWorkStep = 1;       // battery_per_work_step
    int rechargePerStep = 1;          // recharge_per_step
    int agingInterval = 50;           // aging_interval: A in the priority key (default 50)
    int lowBatteryPercent = 30;       // low_battery_percent: low-battery threshold (default 30)
};

// One EVENTS record.
struct Event {
    int       time = 0;
    EventType type = EventType::BLOCK;
    int       refId = 0;              // ID from the file (connection, location, request or robot)
    int       refIndex = NO_INDEX;    // the same thing as an index, resolved by the loader
    int       value = 0;              // new urgency for SET_URGENCY, unused otherwise
    int       line = 0;               // line number in the file (for messages)
};

struct World {
    Str    scenarioName;              // from the optional SCENARIO line
    Params params;

    // --- the map: locations, connections, adjacency, chargers, networkVersion ---
    Environment env;

    // --- fleet, requests, events ---
    DynamicArray<Robot>   robots;     // file order
    DynamicArray<int>     robotsById; // robot indices sorted by robot ID (phase 6 acts in ID order)
    DynamicArray<Request> requests;   // stable-sorted by reportTime
    DynamicArray<Event>   events;     // stable-sorted by time

    // --- ID -> index maps (Report 5.3) ---
    // The plan lists three (locations, robots, requests). connIndex is the
    // fourth: BLOCK/UNBLOCK events name a connection ID (Report 5.4: "O(1)
    // after an O(1) hash lookup").
    HashMap<int> locIndex;
    HashMap<int> connIndex;
    HashMap<int> robotIndex;
    HashMap<int> requestIndex;

    // --- clock and cursors ---
    int currentTime = 0;              // step being processed / next step to run
    int nextRequest = 0;              // first request with reported == false
    int nextEvent = 0;                // first event not yet applied
    bool networkChangedThisStep = false;  // set in phase 1, read by phase 3 (Report 4.2)

    // Network version counter (Report 5.9) lives in env.networkVersion.
    int networkVersion() const { return env.networkVersion; }
};

#endif // RESQGRID_WORLD_H
