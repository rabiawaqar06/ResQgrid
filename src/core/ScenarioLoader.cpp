// File:       src/core/ScenarioLoader.cpp
// Owner:      Rabia
// Purpose:    Parsing and validation of scenario files.
// Depends on: ScenarioLoader.h (World, MergeSort, Types), <fstream>
// Used by:    see ScenarioLoader.h
// Milestone:  M2
#include "ScenarioLoader.h"
#include "../containers/MergeSort.h"
#include <fstream>
#include <iostream>

// Which section the records we are reading belong to.
const int SEC_NONE = 0;
const int SEC_LOCATIONS = 1;
const int SEC_CONNECTIONS = 2;
const int SEC_ROBOTS = 3;
const int SEC_REQUESTS = 4;
const int SEC_EVENTS = 5;
const int SEC_SKIP = 6;     // bad or unknown header: ignore its records

// The five PARAM keys every scenario should have (team decision: a missing
// one uses the default from Params and gives a warning).
const int SPEC_PARAM_COUNT = 5;
const char* const SPEC_PARAMS[SPEC_PARAM_COUNT] = {
    "max_steps", "speed", "battery_per_cost_unit", "battery_per_work_step", "recharge_per_step"
};

static bool earlierRequest(const Request& a, const Request& b) { return a.reportTime < b.reportTime; }
static bool earlierEvent(const Event& a, const Event& b)       { return a.time < b.time; }
static bool smallerInt(const int& a, const int& b)             { return a < b; }

static bool isSpace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';   // '\r' from Windows line endings
}

bool ScenarioLoader::load(const Str& path, World& world) {
    errors.clear();
    warnings.clear();

    // start from an empty world
    world.scenarioName = "";
    world.params = Params();
    world.env.clear();
    world.robots.clear();
    world.robotsById.clear();
    world.requests.clear();
    world.events.clear();
    world.locIndex.init(0);
    world.connIndex.init(0);
    world.robotIndex.init(0);
    world.requestIndex.init(0);
    world.currentTime = 0;
    world.nextRequest = 0;
    world.nextEvent = 0;
    world.networkChangedThisStep = false;

    ifstream file(path);
    if (!file.is_open()) {
        addError(0, "cannot open file " + path);
        return false;
    }

    Str tokens[MAX_TOKENS];
    Str line;
    int lineNo = 0;
    int section = SEC_NONE;
    Str sectionName;
    int expected = 0;       // count written in the section header
    int got = 0;            // records read so far in this section
    int headerLine = 0;
    bool sawEnd = false;
    bool seenParam[SPEC_PARAM_COUNT] = {false, false, false, false, false};

    while (getline(file, line)) {
        lineNo++;
        int n = tokenize(line, tokens, MAX_TOKENS);
        if (n == 0 || tokens[0][0] == '#') {
            continue;   // blank line or comment
        }
        Str word = tokens[0];

        // Every record starts with a number, so a word starting with a letter
        // is a keyword. It ends the current section: check its count.
        bool keyword = (word[0] >= 'A' && word[0] <= 'Z') || (word[0] >= 'a' && word[0] <= 'z');
        if (!keyword) {
            if (section == SEC_NONE) {
                addError(lineNo, "record outside any section");
            } else if (section != SEC_SKIP) {
                got++;
                if (section == SEC_LOCATIONS) {
                    parseLocation(lineNo, tokens, n, world);
                } else if (section == SEC_CONNECTIONS) {
                    parseConnection(lineNo, tokens, n, world);
                } else if (section == SEC_ROBOTS) {
                    parseRobot(lineNo, tokens, n, world);
                } else if (section == SEC_REQUESTS) {
                    parseRequest(lineNo, tokens, n, world);
                } else {
                    parseEvent(lineNo, tokens, n, world);
                }
            }
            continue;
        }

        if (section != SEC_NONE && section != SEC_SKIP && got != expected) {
            addError(headerLine, sectionName + " count is " + to_string(expected) + " but "
                     + to_string(got) + " records follow");
        }
        section = SEC_NONE;

        if (word == "END") {
            sawEnd = true;
            break;
        } else if (word == "SCENARIO") {
            if (n != 2) {
                addError(lineNo, "SCENARIO needs exactly one name");
            } else {
                world.scenarioName = tokens[1];
            }
        } else if (word == "PARAM") {
            for (int i = 0; i < SPEC_PARAM_COUNT; i++) {
                if (n >= 2 && tokens[1] == SPEC_PARAMS[i]) {
                    seenParam[i] = true;
                }
            }
            parseParam(lineNo, tokens, n, world);
        } else if (word == "LOCATIONS" || word == "CONNECTIONS" || word == "ROBOTS"
                   || word == "REQUESTS" || word == "EVENTS") {
            int count = 0;
            if (n != 2 || !parseInt(tokens[1], count) || count < 0) {
                addError(lineNo, "section header " + word + " needs one count (a whole number >= 0)");
                section = SEC_SKIP;
                continue;
            }
            sectionName = word;
            expected = count;
            got = 0;
            headerLine = lineNo;
            if (word == "LOCATIONS") {
                section = SEC_LOCATIONS;
                world.env.locations.reserve(count);
                if (world.locIndex.size() == 0) {
                    world.locIndex.init(count);
                }
            } else if (word == "CONNECTIONS") {
                section = SEC_CONNECTIONS;
                world.env.connections.reserve(count);
                if (world.connIndex.size() == 0) {
                    world.connIndex.init(count);
                }
            } else if (word == "ROBOTS") {
                section = SEC_ROBOTS;
                world.robots.reserve(count);
                if (world.robotIndex.size() == 0) {
                    world.robotIndex.init(count);
                }
            } else if (word == "REQUESTS") {
                section = SEC_REQUESTS;
                world.requests.reserve(count);
                if (world.requestIndex.size() == 0) {
                    world.requestIndex.init(count);
                }
            } else {
                section = SEC_EVENTS;
                world.events.reserve(count);
            }
        } else {
            addError(lineNo, "unknown section keyword " + word);
            section = SEC_SKIP;
        }
    }

    // file ended inside a section (no END line)
    if (section != SEC_NONE && section != SEC_SKIP && got != expected) {
        addError(headerLine, sectionName + " count is " + to_string(expected) + " but "
                 + to_string(got) + " records follow");
    }
    if (!sawEnd) {
        addError(0, "file ends without the END line");
    }

    if (!seenParam[0]) addWarning(0, "PARAM max_steps missing, using default " + to_string(world.params.maxSteps));
    if (!seenParam[1]) addWarning(0, "PARAM speed missing, using default " + to_string(world.params.speed));
    if (!seenParam[2]) addWarning(0, "PARAM battery_per_cost_unit missing, using default " + to_string(world.params.batteryPerCostUnit));
    if (!seenParam[3]) addWarning(0, "PARAM battery_per_work_step missing, using default " + to_string(world.params.batteryPerWorkStep));
    if (!seenParam[4]) addWarning(0, "PARAM recharge_per_step missing, using default " + to_string(world.params.rechargePerStep));

    validateReferences(world);
    sortAndIndex(world);
    return errors.size() == 0;
}

void ScenarioLoader::printMessages(ostream& out) const {
    for (int i = 0; i < errors.size(); i++) {
        out << "error: line " << errors[i].line << ": " << errors[i].text << "\n";
    }
    for (int i = 0; i < warnings.size(); i++) {
        out << "warning: line " << warnings[i].line << ": " << warnings[i].text << "\n";
    }
}

int ScenarioLoader::tokenize(const Str& line, Str tokens[], int maxTokens) {
    int n = 0;
    int i = 0;
    int len = (int)line.size();
    while (i < len && n < maxTokens) {
        while (i < len && isSpace(line[i])) {
            i++;
        }
        if (i >= len) {
            break;
        }
        int start = i;
        while (i < len && !isSpace(line[i])) {
            i++;
        }
        tokens[n] = line.substr(start, i - start);
        n++;
    }
    return n;
}

bool ScenarioLoader::parseInt(const Str& s, int& out) {
    int len = (int)s.size();
    int i = 0;
    bool negative = false;
    if (len > 0 && s[0] == '-') {
        negative = true;
        i = 1;
    }
    if (i >= len) {
        return false;   // "" or just "-"
    }
    long long value = 0;
    while (i < len) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        value = value * 10 + (s[i] - '0');
        if (value > 2147483648LL) {
            return false;   // too big for an int
        }
        i++;
    }
    if (negative) {
        value = -value;
    }
    if (value > 2147483647LL) {
        return false;
    }
    out = (int)value;
    return true;
}

void ScenarioLoader::addError(int line, const Str& text) {
    LoadMessage m;
    m.line = line;
    m.text = text;
    errors.push_back(m);
}

void ScenarioLoader::addWarning(int line, const Str& text) {
    LoadMessage m;
    m.line = line;
    m.text = text;
    warnings.push_back(m);
}

void ScenarioLoader::parseParam(int line, Str tokens[], int n, World& world) {
    if (n != 3) {
        addError(line, "PARAM needs a key and one value");
        return;
    }
    Str key = tokens[1];
    int* target = nullptr;
    int minValue = 0;
    if (key == "max_steps")                  { target = &world.params.maxSteps;           minValue = 0; }
    else if (key == "speed")                 { target = &world.params.speed;              minValue = 1; }
    else if (key == "battery_per_cost_unit") { target = &world.params.batteryPerCostUnit; minValue = 0; }
    else if (key == "battery_per_work_step") { target = &world.params.batteryPerWorkStep; minValue = 0; }
    else if (key == "recharge_per_step")     { target = &world.params.rechargePerStep;    minValue = 1; }
    else if (key == "aging_interval")        { target = &world.params.agingInterval;      minValue = 1; }
    else if (key == "low_battery_percent")   { target = &world.params.lowBatteryPercent;  minValue = 0; }
    else {
        addWarning(line, "unknown PARAM key " + key + " (ignored)");
        return;
    }

    int value = 0;
    if (!parseInt(tokens[2], value)) {
        addError(line, "PARAM " + key + " value '" + tokens[2] + "' is not a whole number");
        return;
    }
    if (value < minValue || (key == "low_battery_percent" && value > 100)) {
        addError(line, "PARAM " + key + " value " + tokens[2] + " is out of range");
        return;
    }
    *target = value;
}

void ScenarioLoader::parseLocation(int line, Str tokens[], int n, World& world) {
    if (n != 3) {
        addError(line, "location needs 3 fields (id name type), found " + to_string(n));
    }
    int id = 0;
    if (!parseInt(tokens[0], id)) {
        addError(line, "location ID '" + tokens[0] + "' is not a number");
        return;
    }
    LocationType type = LocationType::JUNCTION;
    if (n >= 3 && !fromString(tokens[2], type)) {
        addError(line, "location " + tokens[0] + " has unknown type " + tokens[2]);
    }
    if (world.locIndex.contains(id)) {
        addError(line, "duplicate location ID " + tokens[0]);
        return;
    }
    // A bad record with a readable ID is still added (as a JUNCTION), so that
    // roads using it later don't give extra errors. The file is rejected anyway.
    Location loc;
    loc.id = id;
    if (n >= 2) {
        loc.name = tokens[1];
    }
    loc.type = type;
    int index = world.env.addLocation(loc);
    world.locIndex.insert(id, index);
}

void ScenarioLoader::parseConnection(int line, Str tokens[], int n, World& world) {
    if (n != 5) {
        addError(line, "connection needs 5 fields (id a b cost status), found " + to_string(n));
        return;
    }
    int id = 0, a = 0, b = 0, cost = 0;
    if (!parseInt(tokens[0], id) || !parseInt(tokens[1], a) || !parseInt(tokens[2], b)
        || !parseInt(tokens[3], cost)) {
        addError(line, "connection has a field that is not a number");
        return;
    }
    bool ok = true;
    ConnStatus status = ConnStatus::OPEN;
    if (!fromString(tokens[4], status)) {
        addError(line, "connection " + tokens[0] + " has unknown status " + tokens[4]);
        ok = false;
    }
    if (world.connIndex.contains(id)) {
        addError(line, "duplicate connection ID " + tokens[0]);
        ok = false;
    }
    int ai = NO_INDEX, bi = NO_INDEX;
    if (!world.locIndex.find(a, ai)) {
        addError(line, "connection " + tokens[0] + " refers to missing location " + tokens[1]);
        ok = false;
    }
    if (!world.locIndex.find(b, bi)) {
        addError(line, "connection " + tokens[0] + " refers to missing location " + tokens[2]);
        ok = false;
    }
    if (a == b) {
        addError(line, "connection " + tokens[0] + " is a self-loop (" + tokens[1] + " to " + tokens[2] + ")");
        ok = false;
    }
    if (cost <= 0) {
        addError(line, "connection " + tokens[0] + " has cost " + tokens[3] + " (must be > 0)");
        ok = false;
    }
    if (!ok) {
        return;
    }
    Connection c;
    c.id = id;
    c.a = ai;
    c.b = bi;
    c.cost = cost;
    c.status = status;
    int index = world.env.addConnection(c);
    world.connIndex.insert(id, index);
}

void ScenarioLoader::parseRobot(int line, Str tokens[], int n, World& world) {
    if (n != 5) {
        addError(line, "robot needs 5 fields (id start capability max initial), found " + to_string(n));
        return;
    }
    int id = 0, start = 0, maxBattery = 0, battery = 0;
    if (!parseInt(tokens[0], id) || !parseInt(tokens[1], start) || !parseInt(tokens[3], maxBattery)
        || !parseInt(tokens[4], battery)) {
        addError(line, "robot has a field that is not a number");
        return;
    }
    bool ok = true;
    Capability cap = Capability::GENERAL;
    if (!fromString(tokens[2], cap)) {
        addError(line, "robot " + tokens[0] + " has unknown capability " + tokens[2]);
        ok = false;
    }
    if (world.robotIndex.contains(id)) {
        addError(line, "duplicate robot ID " + tokens[0]);
        ok = false;
    }
    int startIndex = NO_INDEX;
    if (!world.locIndex.find(start, startIndex)) {
        addError(line, "robot " + tokens[0] + " starts at missing location " + tokens[1]);
        ok = false;
    }
    if (maxBattery <= 0 || battery < 0 || battery > maxBattery) {
        addError(line, "robot " + tokens[0] + " battery " + tokens[4] + "/" + tokens[3]
                 + " out of range (need 0 <= initial <= max, max > 0)");
        ok = false;
    }
    if (!ok) {
        return;
    }
    world.robots.push_back(Robot(id, startIndex, cap, maxBattery, battery));
    world.robotIndex.insert(id, world.robots.size() - 1);
}

void ScenarioLoader::parseRequest(int line, Str tokens[], int n, World& world) {
    if (n != 6) {
        addError(line, "request needs 6 fields (id time location category urgency duration), found "
                 + to_string(n));
        return;
    }
    int id = 0, time = 0, loc = 0, urgency = 0, duration = 0;
    if (!parseInt(tokens[0], id) || !parseInt(tokens[1], time) || !parseInt(tokens[2], loc)
        || !parseInt(tokens[4], urgency) || !parseInt(tokens[5], duration)) {
        addError(line, "request has a field that is not a number");
        return;
    }
    bool ok = true;
    Category cat = Category::MEDICAL;
    if (!fromString(tokens[3], cat)) {
        addError(line, "request " + tokens[0] + " has unknown category " + tokens[3]);
        ok = false;
    }
    if (world.requestIndex.contains(id)) {
        addError(line, "duplicate request ID " + tokens[0]);
        ok = false;
    }
    int locIndex = NO_INDEX;
    if (!world.locIndex.find(loc, locIndex)) {
        addError(line, "request " + tokens[0] + " is at missing location " + tokens[2]);
        ok = false;
    }
    if (time < 0) {
        addError(line, "request " + tokens[0] + " has negative report time " + tokens[1]);
        ok = false;
    }
    if (urgency < 1 || urgency > 5) {
        addError(line, "request " + tokens[0] + " has urgency " + tokens[4] + " (must be 1 to 5)");
        ok = false;
    }
    if (duration <= 0) {
        addError(line, "request " + tokens[0] + " has duration " + tokens[5] + " (must be > 0)");
        ok = false;
    }
    if (!ok) {
        return;
    }
    Request q(id, time, locIndex, cat, urgency, duration);
    q.state = RequestState::PENDING;
    q.remainingWork = duration;
    world.requests.push_back(q);
    world.requestIndex.insert(id, world.requests.size() - 1);
}

void ScenarioLoader::parseEvent(int line, Str tokens[], int n, World& world) {
    if (n < 3 || n > 4) {
        addError(line, "event needs 3 or 4 fields (time TYPE ref [value]), found " + to_string(n));
        return;
    }
    EventType type = EventType::BLOCK;
    if (!fromString(tokens[1], type)) {
        addError(line, "unknown event type " + tokens[1]);
        return;
    }
    int time = 0, ref = 0;
    if (!parseInt(tokens[0], time) || !parseInt(tokens[2], ref)) {
        addError(line, "event has a field that is not a number");
        return;
    }
    if (time < 0) {
        addError(line, "event has negative time " + tokens[0]);
        return;
    }
    int value = 0;
    if (type == EventType::SET_URGENCY) {
        if (n != 4 || !parseInt(tokens[3], value)) {
            addError(line, "SET_URGENCY needs a new urgency (a number)");
            return;
        }
        if (value < 1 || value > 5) {
            addError(line, "SET_URGENCY urgency " + tokens[3] + " (must be 1 to 5)");
            return;
        }
    } else if (n != 3) {
        addError(line, "event " + tokens[1] + " takes 3 fields, found " + to_string(n));
        return;
    }
    // the reference (ID -> index) is checked later in validateReferences
    Event e;
    e.time = time;
    e.type = type;
    e.refId = ref;
    e.value = value;
    e.line = line;
    world.events.push_back(e);
}

void ScenarioLoader::validateReferences(World& world) {
    for (int i = 0; i < world.events.size(); i++) {
        Event& e = world.events[i];
        Str ref = to_string(e.refId);
        bool found = false;
        if (e.type == EventType::BLOCK || e.type == EventType::UNBLOCK) {
            found = world.connIndex.find(e.refId, e.refIndex);
            if (!found) {
                addError(e.line, toString(e.type) + " refers to missing connection " + ref);
            }
        } else if (e.type == EventType::STATION_DOWN || e.type == EventType::STATION_UP) {
            found = world.locIndex.find(e.refId, e.refIndex);
            if (!found) {
                addError(e.line, toString(e.type) + " refers to missing location " + ref);
            } else if (world.env.locations[e.refIndex].type != LocationType::CHARGING) {
                addError(e.line, toString(e.type) + " on location " + ref + ", which is not a CHARGING station");
            }
        } else if (e.type == EventType::SET_URGENCY) {
            found = world.requestIndex.find(e.refId, e.refIndex);
            if (!found) {
                addError(e.line, "SET_URGENCY refers to missing request " + ref);
            }
        } else {
            found = world.robotIndex.find(e.refId, e.refIndex);
            if (!found) {
                addError(e.line, toString(e.type) + " refers to missing robot " + ref);
            }
        }
        if (e.time > world.params.maxSteps) {
            addWarning(e.line, "event at time " + to_string(e.time) + " is after max_steps "
                       + to_string(world.params.maxSteps) + " and will never happen");
        }
    }
}

void ScenarioLoader::sortAndIndex(World& world) {
    // stable sorts: same time keeps file order
    mergeSort(world.requests, earlierRequest);
    mergeSort(world.events, earlierEvent);

    // robots in ID order: sort the IDs, then look each one up
    DynamicArray<int> ids;
    for (int i = 0; i < world.robots.size(); i++) {
        ids.push_back(world.robots[i].id);
    }
    mergeSort(ids, smallerInt);
    world.robotsById.clear();
    for (int i = 0; i < ids.size(); i++) {
        int index = NO_INDEX;
        world.robotIndex.find(ids[i], index);
        world.robotsById.push_back(index);
    }

    // requests moved, so rebuild their map and the SET_URGENCY references
    world.requestIndex.init(world.requests.size());
    for (int i = 0; i < world.requests.size(); i++) {
        world.requestIndex.insert(world.requests[i].id, i);
    }
    for (int i = 0; i < world.events.size(); i++) {
        if (world.events[i].type == EventType::SET_URGENCY) {
            world.requestIndex.find(world.events[i].refId, world.events[i].refIndex);
        }
    }
}
