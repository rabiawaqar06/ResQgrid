// File:       src/core/ScenarioLoader.h
// Owner:      Rabia
// Purpose:    Reads a scenario file line by line, validates it and builds the
//             World. Collects EVERY error with its line number; the World is
//             only usable if there were no errors (Report 9, 9.4).
// Depends on: World (Environment, Robot, Request, HashMap, DynamicArray),
//             MergeSort, Types
// Used by:    Simulation, main.cpp (prints errors),
//             test_loader
// Milestone:  M2
//
// Format summary (Report 9.1-9.3): '#' lines and blank lines ignored; optional
// SCENARIO name; PARAM key value; sections LOCATIONS / CONNECTIONS / ROBOTS /
// REQUESTS / EVENTS, each "KEYWORD count" followed by exactly count records;
// file ends with END.
//
// Errors reject the file; warnings do not (Report 9.4 table).
// KNOWN DATA ISSUE: data/scenario_04_dynamic_events.txt has duplicate
// connections 35-37 (see README "Known issues"); Rabia fixes the file.
#ifndef RESQGRID_SCENARIO_LOADER_H
#define RESQGRID_SCENARIO_LOADER_H

#include "Types.h"
#include "World.h"
#include "../containers/DynamicArray.h"

struct LoadMessage {
    int line = 0;       // 1-based line number in the file (0 = whole file, e.g. missing END)
    Str text;
};

class ScenarioLoader {
public:
    static const int MAX_TOKENS = 16;   // more fields than this on one line is an error anyway

    // clear previous messages and `world`; read the file line by
    // line; parse PARAM / sections; validate every record (Report 9.4);
    // resolve references (IDs -> indices); stable-sort requests and events by
    // time (MergeSort); fill robotsById; build the ID -> index maps; init
    // request state. Return true only if there were no errors.
    bool load(const Str& path, World& world);

    int errorCount() const   { return errors.size(); }
    int warningCount() const { return warnings.size(); }
    const LoadMessage& error(int i) const   { return errors[i]; }
    const LoadMessage& warning(int i) const { return warnings[i]; }

    // print "error: line <n>: <text>" for every error, then
    // "warning: line <n>: <text>" for every warning.
    void printMessages(ostream& out) const;

    // Split `line` on spaces/tabs into tokens[0..n-1]; returns n (no STL).
    // Public so test_loader can check it on its own.
    // handle multiple spaces and tabs; stop at MAX_TOKENS.
    static int tokenize(const Str& line, Str tokens[], int maxTokens);

    // whole-string integer parse (optional leading '-'); false on
    // "", "12a", "3.5" or overflow.
    static bool parseInt(const Str& s, int& out);

private:
    void addError(int line, const Str& text);
    void addWarning(int line, const Str& text);

    // One parser per record kind; each validates its fields and adds errors.
    // Each checks its record (fields, ranges, enum spellings, duplicates).
    // See: report Sections 9.2 and 9.4
    void parseParam(int line, Str tokens[], int n, World& world);
    void parseLocation(int line, Str tokens[], int n, World& world);
    void parseConnection(int line, Str tokens[], int n, World& world);
    void parseRobot(int line, Str tokens[], int n, World& world);
    void parseRequest(int line, Str tokens[], int n, World& world);
    void parseEvent(int line, Str tokens[], int n, World& world);

    // After reading: cross-checks that need the whole file (event references,
    // station events on CHARGING only, events after max_steps -> warning).
    // See: report Section 9.4
    void validateReferences(World& world);

    // mergeSort requests and events by time; build robotsById
    // (mergeSort of indices by robot ID); rebuild requestIndex after the sort.
    void sortAndIndex(World& world);

    DynamicArray<LoadMessage> errors;
    DynamicArray<LoadMessage> warnings;
};

#endif // RESQGRID_SCENARIO_LOADER_H
