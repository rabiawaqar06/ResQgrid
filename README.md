# ResQGrid: AI Rescue Robots

**Data Structures Lab, Fall 2026 · FAST-NUCES Islamabad · Instructor: Mam Kainat Iqbal**

ResQGrid is a C++17 console simulator that coordinates a fleet of autonomous rescue robots in a
disaster-hit city. It reads the whole scenario from a text file, runs the simulation in discrete
time steps, writes a log line for every change, and prints final statistics. All data
structures are written by us: **no STL containers and no external libraries**. The same input
always gives byte-identical output.

## Team

| Member | Roll no. | Built |
|---|---|---|
| **Rabia Waqar** | 25I-0166 | `HashMap.h`, `MinHeap.h`, `World.h`, `Environment.*`, `Router.*` (Dijkstra with lazy deletion), `ScenarioLoader.*` (parsing + validation), `EventProcessor.*`, `Rerouter.*`, `test_loader.cpp`, `test_router.cpp`, scenario_04 fix. *The map, shortest paths, reading and checking input files, dynamic events and rerouting.* |
| **Sarah Mahmood** | 25I-027 | `IndexedMaxHeap.h`, `Coordinator.*` (assignment), `Movement.*` (movement/battery/recharging), `Simulation.*` (7-phase step loop), `test_coordinator.cpp`, `CMakeLists.txt`, scenario files. *Who serves which request, how robots move and use battery, and the step loop that ties everything together.* |
| **Farwa Abid** | 25I-2083 | `DynamicArray.h`, `LinkedList.h`, `Queue.h`, `MergeSort.h`, `Types.h`, `Robot.*`, `Request.*`, `Logger.*`, `Statistics.*`, `main.cpp`, `test_containers.cpp`. *The basic containers, the robot and request state machines, the log, the statistics and the command-line program.* |

`Types.h` and `World.h` (the shared enums and the world data) were agreed by all three of us on day 1.

---

## Problem overview

A city is a graph: **locations** (junctions, buildings, hospitals, depots, charging stations, a
command post) joined by **undirected weighted roads**, some of which are BLOCKED. **Robots** have a
capability (MEDICAL, FIRE, SUPPLY, SEARCH or GENERAL) and a battery. **Rescue requests** appear
over time with a category, an urgency 1-5 and a work duration. **Events** change the world while it
runs: roads close and reopen, charging stations go down and come back up, urgencies change, robots go
offline and come back. The simulator decides, every step, which robot serves which request, moves the
robots along shortest open paths, manages their batteries, and reacts to every event.

## Features (all working and tested)

- Loads a complete scenario from a file; `data/scenario_provided.txt` loads and runs **unchanged**.
- Validates the input and reports **every** error with its line number, without crashing (exit code 2).
- Map as an adjacency list with shared road records: blocking a road affects both directions at once.
- Dijkstra (binary min-heap, lazy deletion) over OPEN roads only, run **from the request**, so one
  run gives the distance to every robot and to the nearest open charger.
- Pending requests in an **indexed max-heap** with an aging key (no starvation) and change-key for `SET_URGENCY`.
- Deterministic assignment with documented tie-breaks, a battery check with a charger reserve, and conflict counting.
- Battery use for moving and working, recharging at open stations, and a low-battery return rule.
- All 7 event types, applied in file order; rerouting for robots on or heading towards a closed road or a closed charger.
- A full automatic run, or an **interactive step mode** (`--step`) that shows the environment, the robots and the requests at any step.
- A plain-text log (console and/or file), one line per change, plus final statistics.
- Tools: a seeded scenario generator for the performance runs, and an invariant checker.

---

## Build

**Requirements:** a C++17 compiler (g++ 9+, clang, or MSVC from Visual Studio 2019/2022) and CMake 3.16 or newer. No other libraries.

### Linux / macOS
```bash
cmake -S . -B build
cmake --build build
./build/resqgrid data/scenario_provided.txt
```

### Windows (Visual Studio 2022, "Developer PowerShell for VS")
```powershell
cmake -S . -B build
cmake --build build
.\build\Debug\resqgrid.exe data\scenario_provided.txt
```
(The Visual Studio generator puts programs in `build\Debug\`. With `-G Ninja` they go straight into `build\`.)

### Without CMake (one line)
```bash
g++ -std=c++17 -Wall -Wextra -Isrc src/main.cpp src/core/*.cpp src/output/*.cpp -o resqgrid
```
The tools build the same way, for example
`g++ -std=c++17 -Isrc tools/check_invariants.cpp src/core/*.cpp src/output/*.cpp -o check_invariants`
and `g++ -std=c++17 tools/gen_scenario.cpp -o gen_scenario`.

A Release build (`cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release`) is what we used for timing.

---

## Run

```
resqgrid <scenario_file> [--log <path>] [--quiet] [--step]
```

| Option | Meaning |
|---|---|
| *(none)* | Full automatic run: log lines on the console, then the final statistics. |
| `--log <path>` | Also write the log to `<path>` (the file ends with the `STATS` lines). |
| `--quiet` | Do not print log lines on the console (statistics are still printed). |
| `--step` | Interactive step mode (commands below). |

Exit codes: `0` ok, `1` bad command line, `2` invalid scenario file (all errors are printed), `3` the log file cannot be created.

Examples:
```bash
./build/resqgrid data/scenario_provided.txt
./build/resqgrid data/scenario_provided.txt --log output/provided_log.txt --quiet
./build/resqgrid data/scenario_05_stress.txt --log output/scenario_05_log.txt --quiet
./build/resqgrid data/invalid/invalid_02_bad_values_and_references.txt      # lists 18 errors + 1 warning, exit 2
./build/resqgrid data/scenario_01_tiny_trace.txt --step
```

### Step mode commands

| Command | Does |
|---|---|
| `s` | Run one time step and print its log lines. |
| `n <k>` | Run `k` steps. |
| `env` | Environment: every location (chargers shown UP/DOWN), every road with cost and OPEN/BLOCKED, network version. |
| `robots` | Every robot: capability, state, battery/max, location *or* road position (`on road 9 (8->6, 1/3)`), request, target. |
| `requests` | Requests grouped as PENDING / ASSIGNED / IN_PROGRESS / COMPLETED / FAILED / not reported yet, plus the pending max-heap. |
| `stats` | Counts so far; the full final statistics once the run is finished. |
| `log [k]` | The last `k` log lines (default 20), useful together with `--quiet`. |
| `run` | Run to the end. |
| `help`, `q` | Command list; quit. |

The print commands only read the simulation; they never change it. Input can be piped, e.g.
`printf "s\nenv\nrobots\nrequests\nn 5\nstats\nrun\nq\n" | ./build/resqgrid data/scenario_01_tiny_trace.txt --step`
(the output of exactly this is in `output/step_mode_demo.txt`).

### Tests and tools
```bash
ctest --test-dir build                 # Windows (Visual Studio generator): ctest --test-dir build -C Debug
./build/check_invariants data/scenario_05_stress.txt output/scenario_05_log.txt
./build/gen_scenario 1000 2026 data/perf/perf_1000.txt 600
```

---

## Input format

Plain text. Lines starting with `#` and blank lines are ignored. Fields are separated by spaces or
tabs, and names contain no spaces. Windows line endings are accepted. Order: optional
`SCENARIO <name>`, `PARAM` lines, then the sections. Each section is `KEYWORD <count>` followed by
exactly `count` records, and the file ends with `END`.

| Section | Record fields | Allowed values |
|---|---|---|
| `PARAM` | `key value` | see the next table |
| `LOCATIONS` | `id name type` | JUNCTION, BUILDING, HOSPITAL, DEPOT, CHARGING, COMMAND |
| `CONNECTIONS` | `id location_a location_b cost status` | cost > 0, status OPEN or BLOCKED; undirected; no self-loops |
| `ROBOTS` | `id start_location capability max_battery initial_battery` | MEDICAL, FIRE, SUPPLY, SEARCH, GENERAL; max > 0; 0 <= initial <= max |
| `REQUESTS` | `id report_time location category urgency duration_steps` | MEDICAL, FIRE, SUPPLY, SEARCH (not GENERAL); time >= 0; urgency 1-5; duration > 0 |
| `EVENTS` | `time TYPE ref [value]` | `BLOCK`/`UNBLOCK <connection>`, `STATION_DOWN`/`STATION_UP <charging location>`, `SET_URGENCY <request> <1-5>`, `ROBOT_OFFLINE`/`ROBOT_ONLINE <robot>` |

Records may come in any order; requests and events are sorted by time with a **stable** sort, so
records with the same time keep their file order.

| PARAM | Meaning | Default if missing |
|---|---|---|
| `max_steps` | last time step | 0 (and a warning) |
| `speed` | cost units a robot travels per step (>= 1) | 1 (and a warning) |
| `battery_per_cost_unit` | battery used per cost unit travelled | 1 (and a warning) |
| `battery_per_work_step` | battery used per step of work | 1 (and a warning) |
| `recharge_per_step` | battery added per step at an open charger (>= 1) | 1 (and a warning) |
| `aging_interval` | *our extension*: A in the priority key (>= 1) | 50, no warning |
| `low_battery_percent` | *our extension*: low-battery threshold, 0-100 | 30, no warning |

A **missing** spec PARAM is not an error: the default is used and a warning says so (team decision).

### Validation: errors and warnings

Loading never stops at the first problem: every line is checked and **all** messages are listed as
`error: line N: ...` / `warning: line N: ...` (line 0 = about the whole file). Any error rejects
the file (exit code 2), and the simulation does not start.

| Errors (file rejected) | Warnings (file still runs) |
|---|---|
| section count does not match the number of records; missing `END`; section header without a valid count; unknown section keyword; record outside any section | unknown `PARAM` key (ignored) |
| wrong number of fields; text where a number is expected; number overflow | a spec `PARAM` is missing (default used) |
| unknown location type, road status, capability, category or event type (`GENERAL` is not a request category) | an event later than `max_steps` (it can never happen) |
| duplicate location / connection / robot / request ID | *at run time (log `WARN`)*: redundant events (block a blocked road, unblock an open road, station already down/up, robot already offline / not offline, `SET_URGENCY` on a finished request) |
| a road, robot, request or event refers to a missing ID; a road from a location to itself | |
| road cost <= 0; battery out of range; urgency not 1-5; negative report time or event time; duration <= 0; PARAM value out of range | |
| `STATION_DOWN`/`STATION_UP` on a location that is not CHARGING | |

---

## Output format

### Log

One line per change: `t=<step> <KIND> key=value ...`, single spaces, and **file IDs** (never
internal indices). Routes are location IDs joined by `-`. Example (Scenario 01):
```
t=0 REJECT request=2 robot=2 reason=battery need=13 have=12
t=0 ASSIGN request=2 robot=3 route=7-5-6 cost=6
t=0 STATE robot=2 AVAILABLE->RETURNING target=7
t=4 EVENT BLOCK connection=4 (2-5)
t=4 REROUTE robot=1 on=4 back_to=2 route=2-4-7-5 cost=9
t=7 COMPLETE request=2 robot=3 response=7
```

| KIND | Fields | Written when |
|---|---|---|
| `REQUEST` | `request= at= category= urgency=` | a request is reported (becomes PENDING) |
| `EVENT` | `<TYPE> connection=/location=/request=/robot=<id> [detail]` | an event is applied |
| `ASSIGN` | `request= robot= route= cost=` | a robot gets a request |
| `REJECT` | `request= robot= reason=battery need= have=` | a capable, reachable robot fails the battery check |
| `STATE` | `robot=<id> FROM->TO [target=]` or `request=<id> FROM->TO` | a state change not covered by the lines above |
| `REROUTE` | `robot= [on= back_to=] route= cost=` | a robot gets a new route (`on`/`back_to` when it turns back on a closed road) |
| `RELEASE` | `request= robot= reason=offline/no_route` | a request goes back to PENDING (a reassignment) |
| `ARRIVE` | `robot= at=` | a robot reaches the end of its route |
| `COMPLETE` | `request= robot= response=` | a request is finished |
| `FAIL` | `request= reason=unreachable/not_served` | an unfinished request at the end of the run |
| `WARN` | free text | redundant event, robot waiting, no charger reachable |
| `INVALID` | `robot/request=<id> FROM->TO` | a refused state change (never appears in our runs) |
| `STATS` | `key=value ...` | the 3 final statistics lines at the end of a log file |

### Final statistics

Printed after every finished run (and written as `STATS` lines into the log file):

| Field | Definition |
|---|---|
| completed / failed / remaining | requests COMPLETED / FAILED (split into `unreachable` and `not served`) / still open when the run stopped (0 after a normal end) |
| average / max response | response time = completion step - report time, over completed requests |
| robot utilization % | 100 x (robot-steps spent busy) / (robots x steps run); *busy* = any state except AVAILABLE and OFFLINE (travelling, working, returning, recharging) |
| total distance / battery used | cost units travelled / battery spent by all robots |
| conflicts | assignments where more than one robot was a valid candidate |
| reassignments | requests released back to PENDING (robot went offline, or no route left) |
| reroutes | new routes given by the Rerouter |
| battery rejections | `REJECT` lines (capable, reachable, free robot without enough battery) |
| Dijkstra runs | calls of `Router::run` (assignment, low-battery check, rerouting) |
| execution time (ms) | time spent inside the simulation steps (printed on the console only, **not** in the log, so two logs can be compared byte by byte) |

---

## Simulation rules

**One step at time t has 7 phases, always in this order:**
1. **Events** with time t, in file order.
2. **New requests** with `report_time == t` become PENDING and enter the priority queue, in file order.
3. **Reroute check**: only if a road or station changed in phase 1, and only after *all* events of the
   step, so a BLOCK and UNBLOCK of the same road in one step cause no rerouting.
4. **Assignment** of pending requests to available robots.
5. **Low battery**: an AVAILABLE robot below `low_battery_percent` of its maximum heads to the nearest open charger.
6. **Action**: robots act in increasing ID order: move, work or recharge.
7. **Record**: the log lines of the step have been written; the counters are updated.

**Movement.** A robot moves one cost unit at a time, up to `speed` units per step, paying
`battery_per_cost_unit` per unit. It can pass a location and continue in the same step. A robot
that reaches its request starts working in the **next** step. It works one unit per step
(`battery_per_work_step`) until `duration_steps` are done. A robot never starts onto a BLOCKED road.

**Battery and charging.** At an open charging station a RECHARGING robot gains `recharge_per_step`
per step until it is full, then becomes AVAILABLE. Battery never goes below 0 or above the maximum.

**Feasibility.** A robot may take a request only if
`battery >= bc * (d(robot, request) + d(request, nearest open charger)) + bw * duration`.
The charger term is 0 if no open charger is reachable from the request. Every failed check is
logged as a `REJECT` and counted.

**Priority.** key = `aging_interval * urgency - report_time`. A larger key goes first; on equal
keys, the smaller request ID goes first. Older requests win over newer ones of the same urgency,
so nothing starves, and the key never needs to be recomputed as time passes. `SET_URGENCY` changes
the key in place (change-key).

**Choosing a robot.** Candidates are AVAILABLE, capable (same category or GENERAL), reachable and
feasible. Tie-breaks, in order:
1. smallest travel cost;
2. specialist before GENERAL;
3. higher battery;
4. smaller robot ID.

More than one candidate counts as a **conflict**. A request that cannot be served is skipped, so it
never blocks the requests behind it.

**No route.** If no capable robot can reach a request, it stays PENDING, is marked *unreachable*
and is not checked again until the network version changes. The version goes up on every UNBLOCK.

**Rerouting** (only in a step where the network changed):
- a robot **on** the closed road turns back and walks to the location it came from (paying battery), then follows a new route;
- a robot whose route uses a closed road **later on** gets a new route from its next location;
- a RETURNING or RECHARGING robot whose charger went down is sent to the nearest other open charger;
- if no feasible route exists, the request goes back to PENDING (`RELEASE ... reason=no_route`, a
  reassignment), and the robot becomes AVAILABLE, or RETURNING if its battery is low.

**Robot offline / online.** An offline robot's request goes back to PENDING with its remaining
work kept (`RELEASE ... reason=offline`), so another robot can finish it. A robot that goes
offline **on a road stays where it is** until it comes back online. It then becomes AVAILABLE at
the location it had left (team decision).

**Termination.** The run stops after the step where `t == max_steps`, or after the first step
where every request is COMPLETED or FAILED and no requests or events are left. Every unfinished
request then becomes FAILED with reason `unreachable` (marked unreachable) or `not_served`.

**Team decisions.**
- Chargers at equal distance: the smaller location ID wins.
- A robot that goes offline on a road stays there until online.
- A missing PARAM gets its default plus a warning.
- Events at the same time are applied in file order (stable sort).

---

## Data structures

| Structure | Where it is used | Why | Complexity |
|---|---|---|---|
| `DynamicArray<T>` (growable array, doubling) | locations, roads, robots, requests, events, Dijkstra `dist[]`/`pred[]` | everything is referred to by index, so we need O(1) access | push_back O(1) amortised, `[]` O(1) |
| `LinkedList<T>` (head + tail) | adjacency lists, hash buckets, storage behind `Queue` | cheap insert at both ends, no resizing | push front/back O(1), pop front O(1), walk O(n) |
| `Queue<T>` (on LinkedList) | a robot's route (connection indices) | a route is consumed from the front; rerouting replaces it | enqueue / dequeue O(1) |
| `HashMap<int>` (separate chaining, table size = smallest prime >= 2n) | file ID -> index for locations, roads, robots, requests | IDs in files are arbitrary numbers; validation needs fast duplicate checks | insert / find O(1) average |
| Adjacency list with **shared road records** | the map (`Environment`) | sparse graph (E ~ 2V); both ends point to the same road, so a BLOCK hides it in both directions in O(1) | memory O(V + E), neighbours O(degree), block/unblock O(1) |
| `MinHeap` (binary heap) | Dijkstra, with **lazy deletion** (stale entries are skipped when popped) | no decrease-key needed, simple and fast | push / pop O(log n); Dijkstra O((V + E) log V) |
| `IndexedMaxHeap` (heap + position array) | pending requests by priority key | highest priority in O(log n), and `SET_URGENCY` needs change-key | insert / extract-max / change-key O(log n), contains O(1) |
| Stable `mergeSort` | sorting requests and events by time, robot indices by ID | the spec requires same-time records to keep file order | O(n log n), stable |

**Alternatives we considered.**
- **Adjacency matrix:** O(V²) memory (1M cells at 1000 locations) for a graph with only about 2V roads, and scanning a row costs O(V). The list wins.
- **Floyd-Warshall:** O(V³) all-pairs precomputation that every BLOCK/UNBLOCK would invalidate. On-demand Dijkstra costs O((V + E) log V), and one run **from the request** gives the distance to every robot at once.
- **Sorted list for pending requests:** O(n) insert and O(n) urgency change, against O(log n) in the heap.
- **BFS / A\*:** BFS ignores road costs. A\* needs a heuristic, and our locations have no coordinates; it also finds one target, while we need distances to all robots and chargers.

---

## Scenarios

| File | Locations / roads / robots / requests / events, max_steps | What it tests |
|---|---|---|
| `data/scenario_provided.txt` | 56 / 117 (7 blocked) / 12 / 60 / 20, 250 | the course's minimum scenario; loads and runs unchanged |
| `data/scenario_01_tiny_trace.txt` | 8 / 11 / 3 / 4 / 1, 60 | full hand trace: routes, battery rejections, low-battery return, turn-back reroute |
| `data/scenario_02_cutoff_locations.txt` | 12 / 18 / 4 / 6 / 3, 150 | unreachable requests stay PENDING, no head-of-line blocking, zero-length route, retry after UNBLOCK |
| `data/scenario_03_ties_and_conflicts.txt` | 10 / 14 / 6 / 10 / 0, 200 | every tie-break rule, priority order, conflict counting |
| `data/scenario_04_dynamic_events.txt` | 20 / 34 / 6 / 24 / 13, 200 | every event type, unsorted events (stable sort), same-step BLOCK+UNBLOCK, redundant events |
| `data/scenario_05_stress.txt` | 120 / 260 (12 blocked) / 24 / 150 / 60, 600 | stress test: at least twice the provided file in every dimension |
| `data/invalid/invalid_01_count_mismatch.txt` | | 2 count mismatches (lines 27, 41) |
| `data/invalid/invalid_02_bad_values_and_references.txt` | | 18 errors + 1 warning (bad values, duplicates, missing references) |
| `data/invalid/invalid_03_broken_structure.txt` | | 7 errors (non-numeric PARAM, unknown section, missing/extra fields, header without count, text for an ID, no END) + 4 warnings |
| `data/perf/perf_056.txt` ... `perf_1000.txt` | 56 / 120 / 250 / 500 / 1000 locations, max_steps 600 | performance runs; made by `tools/gen_scenario <n> 2026 <file> 600` |

---

## Results

### Provided scenario (`output/provided_stats.txt`, log in `output/provided_log.txt`)

| Metric | Value |
|---|---|
| steps run | 211 (all requests finished before max_steps 250) |
| completed / failed / remaining | 60 / 0 / 0 |
| average / max response | 19.43 / 94 steps |
| robot utilization | 50.75 % |
| total distance / battery used | 785 / 1383 |
| conflicts / reassignments / reroutes | 33 / 2 / 0 |
| battery rejections / Dijkstra runs | 19 / 85 |

Scenario 01 matches the hand trace exactly: 4 completed, 0 failed, 1 reroute, 2 battery
rejections, response times 16 / 7 / 26 / 15, end at t=28. Two runs of the same file give
byte-identical logs.

### Performance (`output/performance_results.md`)

Release build, median of 3 runs, Intel Core i5-6300U, Windows 10:

| Locations | Roads | Robots | Requests | Events | Steps run | Sim time (ms) | ms / step | Dijkstra runs | Completed / failed |
|---|---|---|---|---|---|---|---|---|---|
| 56 | 106 | 11 | 70 | 28 | 360 | 1.63 | 0.0045 | 78 | 70 / 0 |
| 120 | 237 | 24 | 150 | 60 | 372 | 3.46 | 0.0093 | 171 | 150 / 0 |
| 250 | 504 | 50 | 312 | 125 | 373 | 13.91 | 0.0373 | 350 | 312 / 0 |
| 500 | 1031 | 100 | 625 | 250 | 372 | 57.24 | 0.1539 | 697 | 625 / 0 |
| 1000 | 2087 | 200 | 1250 | 500 | 380 | 221.08 | 0.5818 | 1401 | 1250 / 0 |

Doubling every dimension multiplies the time by about 4. The number of Dijkstra runs grows
linearly with the requests, and each run costs O((V + E) log V). Even 1000 locations need about
0.6 ms per step.

---

## Project structure

```
resqgrid/
|-- CMakeLists.txt                 Sarah     builds resqgrid, 5 tests, 2 tools
|-- README.md                      Farwa
|-- .gitignore                     Sarah
|-- src/
|   |-- main.cpp                   Farwa     command line, full run, --step mode
|   |-- containers/
|   |   |-- DynamicArray.h         Farwa
|   |   |-- LinkedList.h           Farwa
|   |   |-- Queue.h                Farwa
|   |   |-- MergeSort.h            Farwa
|   |   |-- HashMap.h              Rabia
|   |   |-- MinHeap.h              Rabia
|   |   `-- IndexedMaxHeap.h       Sarah
|   |-- core/
|   |   |-- Types.h                shared    enums, toString/fromString, Str
|   |   |-- World.h                shared    all simulation data
|   |   |-- Robot.h/.cpp           Farwa
|   |   |-- Request.h/.cpp         Farwa
|   |   |-- Environment.h/.cpp     Rabia
|   |   |-- Router.h/.cpp          Rabia
|   |   |-- ScenarioLoader.h/.cpp  Rabia
|   |   |-- EventProcessor.h/.cpp  Rabia
|   |   |-- Rerouter.h/.cpp        Rabia
|   |   |-- Coordinator.h/.cpp     Sarah
|   |   |-- Movement.h/.cpp        Sarah
|   |   `-- Simulation.h/.cpp      Sarah
|   `-- output/
|       |-- Logger.h/.cpp          Farwa
|       `-- Statistics.h/.cpp      Farwa
|-- tests/
|   |-- test_containers.cpp        Farwa
|   |-- test_robot_request.cpp     Farwa
|   |-- test_loader.cpp            Rabia
|   |-- test_router.cpp            Rabia
|   `-- test_coordinator.cpp       Sarah
|-- tools/
|   |-- gen_scenario.cpp           Rabia     seeded scenario generator
|   `-- check_invariants.cpp       Rabia     invariant checker
|-- data/                          Sarah     scenario_provided, scenario_01..05, invalid/, perf/
`-- output/                                  submitted logs, statistics and performance results
```

`output/` contains `provided_log.txt` and `provided_stats.txt` (the required output of the provided
scenario), `scenario_0X_log.txt` / `_stats.txt`, `invalid_0X_errors.txt`, `step_mode_demo.txt`,
`invariants_check.txt` and `performance_results.md`.

---

## Testing

Each test file is its own program (own `main`, own CMake target); `ctest` runs all five. A failed
check prints the file and line and makes the program exit with 1.

| Test | Checks |
|---|---|
| `test_containers` (59 checks) | DynamicArray (10,000 pushes, deep copy, self-assignment, reserve, assign, pop_back, push of its own element while growing), LinkedList (order, pop, deep copy, reuse after empty), Queue (FIFO, front, copy), MergeSort (sorted, **stable**, empty, one element, 1000 reversed), Types (every enum round-trips; HOUSE, PILOT, GENERAL-as-category rejected) |
| `test_robot_request` (90 checks) | capability rule, the full robot transition table (15 allowed), request transitions (COMPLETED/FAILED final), priority key (Scenario 01 values 145 and 98, no int overflow), every Logger line format, ring buffer, log file, Statistics (Scenario 01 response times, utilization, reset, no division by zero) |
| `test_loader` (83 checks) | HashMap (collisions, negative keys, duplicates), tokenizer and number parser, the exact error/warning **line numbers** of all three invalid files, record counts and sort order of every valid file, stable event order in Scenario 04, reloading |
| `test_router` (107 checks) | MinHeap, Environment block/unblock in both directions and the network version, Router (unreachable, zero-length route, both directions, nearest open charger) and every key cost in Scenario 01 |
| `test_coordinator` (52 checks) | IndexedMaxHeap (order, ties, change-key), Scenario 03 assignment order and tie-breaks, full Scenario 01 run against the hand trace |

`tools/check_invariants` runs the real simulation step by step and checks after every step:
- battery always between 0 and the maximum;
- no robot enters, crosses or keeps going forward on a BLOCKED road;
- no request held by two robots;
- a request completes at most once.

Given a log file, it also checks that each request has at most one `COMPLETE` line and that there
are no `INVALID` lines. Results for every scenario are in `output/invariants_check.txt` (all hold).
All tests and scenarios also run clean under AddressSanitizer, with no leaks.

---

## Known limitations

- **Greedy assignment:** each request takes the best robot available *now*; there is no global
  optimisation over all requests and robots.
- **No pre-emption:** a robot keeps its request even if a more urgent one appears later.
- **The battery reserve assumes the nearest charger stays open:** if it goes down later, the robot
  is redirected to another one, which may be further away.
- **Linear reroute scan:** when the network changes, every moving robot's remaining route is scanned (O(R x route length)).
- **Repeated closures:** if the network changes again while a robot is walking back on a closed
  road, the robot is re-planned from its current position again.
- **Equal-cost routes:** Dijkstra keeps the first shortest path it finds. In Scenario 04, robot 2
  therefore takes an equally short route that does not use road 29, so the "BLOCK 29 at t=3"
  reroute described in the file's header comment does not happen.
- **No recharging in the arrival step:** a robot that reaches a charger starts recharging in the
  next step. In Scenario 04, robot 6 therefore has 20 battery (not the 24 in the header comment)
  when its station goes down at t=6; it still goes to charger 5 as expected.
