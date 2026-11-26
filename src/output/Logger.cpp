// File:       src/output/Logger.cpp
// Owner:      Farwa
// Purpose:    Formatting and writing log lines; ring buffer of recent lines.
// Depends on: Logger.h (Types)
// Used by:    see Logger.h
// Milestone:  M2
#include "Logger.h"
#include <iostream>

Logger::Logger()
    : toConsole(true), toFile(false), currentTime(0), recentStart(0), recentSize(0) {}

Logger::~Logger() {
    if (toFile) {
        file.close();
        toFile = false;
    }
}

bool Logger::openFile(const Str& path) {
    if (toFile) {
        file.close();
    }
    file.open(path);
    toFile = file.is_open();
    return toFile;
}

void Logger::write(const Str& body) {
    Str line = "t=" + to_string(currentTime) + " " + body;
    if (toConsole) {
        cout << line << "\n";
    }
    if (toFile) {
        file << line << "\n";
    }
    remember(line);
}

// Each helper builds the body string and calls write(). Formats are in the
// README "Log format".

void Logger::assign(int requestId, int robotId, const Str& route, int cost) {
    write("ASSIGN request=" + to_string(requestId) + " robot=" + to_string(robotId)
          + " route=" + route + " cost=" + to_string(cost));
}

void Logger::reject(int requestId, int robotId, int need, int have) {
    write("REJECT request=" + to_string(requestId) + " robot=" + to_string(robotId)
          + " reason=battery need=" + to_string(need) + " have=" + to_string(have));
}

void Logger::robotState(int robotId, RobotState from, RobotState to, int targetLocId) {
    Str text = "STATE robot=" + to_string(robotId) + " " + toString(from) + "->" + toString(to);
    if (targetLocId != NO_INDEX) {
        text = text + " target=" + to_string(targetLocId);
    }
    write(text);
}

void Logger::requestState(int requestId, RequestState from, RequestState to) {
    write("STATE request=" + to_string(requestId) + " " + toString(from) + "->" + toString(to));
}

void Logger::newRequest(int requestId, int locationId, Category cat, int urgency) {
    write("REQUEST request=" + to_string(requestId) + " at=" + to_string(locationId)
          + " category=" + toString(cat) + " urgency=" + to_string(urgency));
}

void Logger::event(EventType type, int refId, const Str& detail) {
    Str refName = "robot";
    if (type == EventType::BLOCK || type == EventType::UNBLOCK) {
        refName = "connection";
    } else if (type == EventType::STATION_DOWN || type == EventType::STATION_UP) {
        refName = "location";
    } else if (type == EventType::SET_URGENCY) {
        refName = "request";
    }
    Str text = "EVENT " + toString(type) + " " + refName + "=" + to_string(refId);
    if (detail != "") {
        text = text + " " + detail;
    }
    write(text);
}

void Logger::reroute(int robotId, int onConnId, int backToLocId, const Str& route, int cost) {
    Str text = "REROUTE robot=" + to_string(robotId);
    if (onConnId != NO_INDEX) {
        text = text + " on=" + to_string(onConnId);
    }
    if (backToLocId != NO_INDEX) {
        text = text + " back_to=" + to_string(backToLocId);
    }
    text = text + " route=" + route + " cost=" + to_string(cost);
    write(text);
}

void Logger::release(int requestId, int robotId, const Str& reason) {
    write("RELEASE request=" + to_string(requestId) + " robot=" + to_string(robotId)
          + " reason=" + reason);
}

void Logger::arrive(int robotId, int locationId) {
    write("ARRIVE robot=" + to_string(robotId) + " at=" + to_string(locationId));
}

void Logger::complete(int requestId, int robotId, int response) {
    write("COMPLETE request=" + to_string(requestId) + " robot=" + to_string(robotId)
          + " response=" + to_string(response));
}

void Logger::fail(int requestId, FailReason reason) {
    write("FAIL request=" + to_string(requestId) + " reason=" + toString(reason));
}

void Logger::warning(const Str& message) {
    write("WARN " + message);
}

void Logger::invalidTransition(const Str& what, int id, const Str& from, const Str& to) {
    write("INVALID " + what + "=" + to_string(id) + " " + from + "->" + to);
}

const Str& Logger::recent(int i) const {
    return recentLines[(recentStart + i) % RECENT_CAPACITY];
}

void Logger::remember(const Str& line) {
    if (recentSize < RECENT_CAPACITY) {
        recentLines[(recentStart + recentSize) % RECENT_CAPACITY] = line;
        recentSize++;
    } else {
        recentLines[recentStart] = line;    // overwrite the oldest line
        recentStart = (recentStart + 1) % RECENT_CAPACITY;
    }
}
