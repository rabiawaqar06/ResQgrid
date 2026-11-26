// File:       tests/test_robot_request.cpp
// Owner:      Farwa
// Purpose:    Unit tests for Robot, Request, Logger and Statistics
//             (state machines, capability rule, priority key, log line
//             formats, final metrics). Own main(), own CMake target.
// Depends on: Types, Robot, Request, Logger, Statistics
// Used by:    ctest / "before a PR" check
// Milestone:  M2 / M3
#include <iostream>
#include "core/Types.h"
#include "core/Robot.h"
#include "core/Request.h"
#include "output/Logger.h"
#include "output/Statistics.h"

static int g_checks = 0, g_failed = 0, g_todo = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; \
    cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)

static Str lastLine(const Logger& log) {
    if (log.recentCount() == 0) {
        return "";
    }
    return log.recent(log.recentCount() - 1);
}

static void testRobot() {
    Robot r(4, 0, Capability::FIRE, 120, 100);
    CHECK(r.id == 4 && r.location == 0 && r.battery == 100 && r.maxBattery == 120);
    CHECK(r.state == RobotState::AVAILABLE && !r.isOnRoad() && !r.isMoving());
    CHECK(r.canServe(Category::FIRE) && !r.canServe(Category::MEDICAL));

    Robot g(10, 0, Capability::GENERAL, 110, 110);
    CHECK(g.canServe(Category::MEDICAL) && g.canServe(Category::FIRE)
          && g.canServe(Category::SUPPLY) && g.canServe(Category::SEARCH));

    // normal job cycle
    CHECK(r.setState(RobotState::TRAVELLING));
    CHECK(r.isMoving());
    CHECK(!r.setState(RobotState::RECHARGING));
    CHECK(r.state == RobotState::TRAVELLING);       // unchanged after a refusal
    CHECK(r.setState(RobotState::PERFORMING));
    CHECK(r.setState(RobotState::OFFLINE));
    CHECK(!r.setState(RobotState::TRAVELLING));
    CHECK(!r.setState(RobotState::OFFLINE));        // already offline
    CHECK(r.setState(RobotState::AVAILABLE));

    // recharge cycle
    CHECK(r.setState(RobotState::RETURNING));
    CHECK(!r.setState(RobotState::AVAILABLE));
    CHECK(r.setState(RobotState::RECHARGING));
    CHECK(r.setState(RobotState::RETURNING));       // station went down
    CHECK(r.setState(RobotState::RECHARGING));
    CHECK(r.setState(RobotState::AVAILABLE));

    // the full table
    typedef RobotState S;
    CHECK(Robot::isValidTransition(S::TRAVELLING, S::AVAILABLE));
    CHECK(Robot::isValidTransition(S::TRAVELLING, S::RETURNING));
    CHECK(Robot::isValidTransition(S::RECHARGING, S::OFFLINE));
    CHECK(!Robot::isValidTransition(S::AVAILABLE, S::PERFORMING));
    CHECK(!Robot::isValidTransition(S::AVAILABLE, S::RECHARGING));
    CHECK(!Robot::isValidTransition(S::PERFORMING, S::TRAVELLING));
    CHECK(!Robot::isValidTransition(S::AVAILABLE, S::AVAILABLE));
    int allowed = 0;
    for (int f = 0; f < 6; f++) {
        for (int t = 0; t < 6; t++) {
            if (Robot::isValidTransition((S)f, (S)t)) {
                allowed++;
            }
        }
    }
    CHECK(allowed == 15);   // 10 listed arrows + 5 "other state -> OFFLINE"

    // each robot copy has its own route
    DynamicArray<Robot> fleet;
    Robot withRoute(1, 0, Capability::MEDICAL, 100, 100);
    withRoute.route.enqueue(5);
    withRoute.route.enqueue(8);
    for (int i = 0; i < 10; i++) {
        fleet.push_back(withRoute);
    }
    int v = 0;
    fleet[0].route.dequeue(v);
    CHECK(v == 5 && fleet[0].route.size() == 1 && fleet[9].route.size() == 2);
}

static void testRequest() {
    Request q(1, 3, 0, Category::MEDICAL, 4, 2);
    CHECK(q.state == RequestState::PENDING && !q.reported && q.remainingWork == 2);
    CHECK(q.priorityKey(50) == 197);

    // Scenario 01 (default aging 50): Req 4 = 50*3-5 = 145, Req 3 = 50*2-2 = 98
    Request q3(3, 2, 4, Category::SUPPLY, 2, 2);
    Request q4(4, 5, 5, Category::SEARCH, 3, 2);
    CHECK(q4.priorityKey(50) == 145);
    CHECK(q3.priorityKey(50) == 98);
    CHECK(q4.priorityKey(1000000000) == 3000000000LL - 5);   // no int overflow

    CHECK(!q.setState(RequestState::IN_PROGRESS));
    CHECK(q.setState(RequestState::ASSIGNED));
    CHECK(q.setState(RequestState::PENDING));        // robot offline / no route
    CHECK(q.setState(RequestState::ASSIGNED));
    CHECK(q.setState(RequestState::IN_PROGRESS));
    CHECK(!q.setState(RequestState::FAILED));        // only PENDING -> FAILED
    CHECK(q.setState(RequestState::PENDING));        // work kept
    CHECK(q.setState(RequestState::ASSIGNED));
    CHECK(q.setState(RequestState::IN_PROGRESS));
    CHECK(q.setState(RequestState::COMPLETED));
    CHECK(q.isFinished());
    CHECK(!q.setState(RequestState::PENDING));       // completed is final
    CHECK(q.state == RequestState::COMPLETED);

    Request f(2, 0, 0, Category::FIRE, 1, 1);
    CHECK(f.setState(RequestState::FAILED) && f.isFinished());
    CHECK(!f.setState(RequestState::ASSIGNED));

    q.completionTime = 10;
    CHECK(q.responseTime() == 7);
}

static void testLogger() {
    Logger log;
    log.setConsole(false);
    CHECK(log.recentCount() == 0);

    log.setTime(0);
    log.assign(2, 3, "7-5-6", 6);
    CHECK(lastLine(log) == "t=0 ASSIGN request=2 robot=3 route=7-5-6 cost=6");
    log.reject(1, 2, 15, 12);
    CHECK(lastLine(log) == "t=0 REJECT request=1 robot=2 reason=battery need=15 have=12");
    log.robotState(2, RobotState::AVAILABLE, RobotState::RETURNING, 7);
    CHECK(lastLine(log) == "t=0 STATE robot=2 AVAILABLE->RETURNING target=7");
    log.robotState(2, RobotState::RETURNING, RobotState::RECHARGING);
    CHECK(lastLine(log) == "t=0 STATE robot=2 RETURNING->RECHARGING");
    log.requestState(1, RequestState::PENDING, RequestState::ASSIGNED);
    CHECK(lastLine(log) == "t=0 STATE request=1 PENDING->ASSIGNED");
    log.newRequest(3, 4, Category::SEARCH, 3);
    CHECK(lastLine(log) == "t=0 REQUEST request=3 at=4 category=SEARCH urgency=3");

    log.setTime(4);
    log.event(EventType::BLOCK, 4, "(2-5)");
    CHECK(lastLine(log) == "t=4 EVENT BLOCK connection=4 (2-5)");
    log.event(EventType::STATION_DOWN, 7, "");
    CHECK(lastLine(log) == "t=4 EVENT STATION_DOWN location=7");
    log.event(EventType::SET_URGENCY, 3, "urgency=5");
    CHECK(lastLine(log) == "t=4 EVENT SET_URGENCY request=3 urgency=5");
    log.event(EventType::ROBOT_OFFLINE, 2, "");
    CHECK(lastLine(log) == "t=4 EVENT ROBOT_OFFLINE robot=2");
    log.reroute(1, 4, 2, "2-4-7-5", 9);
    CHECK(lastLine(log) == "t=4 REROUTE robot=1 on=4 back_to=2 route=2-4-7-5 cost=9");
    log.reroute(1, NO_INDEX, NO_INDEX, "5-7", 4);
    CHECK(lastLine(log) == "t=4 REROUTE robot=1 route=5-7 cost=4");
    log.release(3, 2, "offline");
    CHECK(lastLine(log) == "t=4 RELEASE request=3 robot=2 reason=offline");
    log.arrive(3, 6);
    CHECK(lastLine(log) == "t=4 ARRIVE robot=3 at=6");
    log.complete(2, 3, 7);
    CHECK(lastLine(log) == "t=4 COMPLETE request=2 robot=3 response=7");
    log.fail(5, FailReason::UNREACHABLE);
    CHECK(lastLine(log) == "t=4 FAIL request=5 reason=unreachable");
    log.warning("redundant BLOCK");
    CHECK(lastLine(log) == "t=4 WARN redundant BLOCK");
    log.invalidTransition("robot", 2, "OFFLINE", "TRAVELLING");
    CHECK(lastLine(log) == "t=4 INVALID robot=2 OFFLINE->TRAVELLING");
    CHECK(log.recentCount() == 18);
    CHECK(log.recent(0) == "t=0 ASSIGN request=2 robot=3 route=7-5-6 cost=6");

    // ring buffer: after 250 lines, recent(0) is line 51
    Logger ring;
    ring.setConsole(false);
    for (int i = 1; i <= 250; i++) {
        ring.setTime(i);
        ring.write("X");
    }
    CHECK(ring.recentCount() == Logger::RECENT_CAPACITY);
    CHECK(ring.recent(0) == "t=51 X");
    CHECK(ring.recent(ring.recentCount() - 1) == "t=250 X");

    // file output
    Logger fileLog;
    fileLog.setConsole(false);
    CHECK(!fileLog.openFile("no_such_folder_xyz/log.txt"));
    CHECK(fileLog.openFile(Str(RESQGRID_DATA_DIR) + "/../output/test_robot_request_log.txt"));
    fileLog.write("HELLO");
}

static void testStatistics() {
    DynamicArray<Robot> robots;
    DynamicArray<Request> requests;
    int reportTimes[4] = { 0, 0, 2, 5 };
    int doneTimes[4] = { 16, 7, 28, 20 };

    Robot r(1, 0, Capability::GENERAL, 100, 100);
    r.distanceTravelled = 20;
    r.batteryUsed = 20;
    r.busySteps = 2;
    robots.push_back(r);
    Robot idle(2, 0, Capability::GENERAL, 100, 100);
    robots.push_back(idle);

    for (int i = 0; i < 4; i++) {
        Request q(i + 1, reportTimes[i], 0, Category::SEARCH, 3, 1);
        q.state = RequestState::COMPLETED;
        q.completionTime = doneTimes[i];
        requests.push_back(q);
    }
    Request lost(5, 0, 0, Category::FIRE, 1, 1);
    lost.state = RequestState::FAILED;
    lost.failReason = FailReason::UNREACHABLE;
    requests.push_back(lost);
    Request late(6, 0, 0, Category::FIRE, 1, 1);
    late.state = RequestState::FAILED;
    late.failReason = FailReason::NOT_SERVED;
    requests.push_back(late);
    Request open(7, 0, 0, Category::FIRE, 1, 1);
    requests.push_back(open);

    Statistics stats;
    stats.addCounters(1, 3, 2, 2, 9);
    stats.finalize(robots, requests, 2, 1.5);
    CHECK(stats.completed == 4);
    CHECK(stats.failed == 2 && stats.failedUnreachable == 1 && stats.failedNotServed == 1);
    CHECK(stats.remaining == 1);
    CHECK(stats.maxResponse == 26);
    CHECK(stats.avgResponse == 16.0);      // (16 + 7 + 26 + 15) / 4
    CHECK(stats.utilization == 50.0);      // 2 busy steps / (2 robots * 2 steps)
    CHECK(stats.totalDistance == 20 && stats.totalBatteryUsed == 20);
    CHECK(stats.reroutes == 1 && stats.conflicts == 3 && stats.reassignments == 2);
    CHECK(stats.batteryRejections == 2 && stats.dijkstraRuns == 9);
    CHECK(stats.stepsRun == 2 && stats.runTimeMs == 1.5);

    Logger log;
    log.setConsole(false);
    stats.writeToLog(log);
    CHECK(log.recentCount() == 3);
    CHECK(log.recent(0) == "t=0 STATS completed=4 failed=2 unreachable=1 not_served=1 remaining=1 steps=2");

    stats.reset();
    CHECK(stats.completed == 0 && stats.failed == 0 && stats.reroutes == 0);
    CHECK(stats.utilization == 0.0 && stats.totalDistance == 0 && stats.stepsRun == 0);

    // no robots / no requests: no division by zero
    DynamicArray<Robot> noRobots;
    DynamicArray<Request> noRequests;
    stats.finalize(noRobots, noRequests, 0, 0.0);
    CHECK(stats.avgResponse == 0.0 && stats.utilization == 0.0);
}

int main() {
    cout << "test_robot_request\n";
    testRobot();
    testRequest();
    testLogger();
    testStatistics();
    cout << g_checks << " checks, " << g_failed << " failed, " << g_todo << " tests still TODO\n";
    return g_failed == 0 ? 0 : 1;
}
