// File:       tests/test_containers.cpp
// Owner:      Farwa
// Purpose:    Unit tests for Farwa's containers: DynamicArray, LinkedList,
//             Queue, MergeSort (+ Types.h round-trips). Own main(), own CMake target.
// Depends on: DynamicArray, LinkedList, Queue, MergeSort, Types
// Used by:    ctest / "before a PR" check
// Milestone:  M1 (all container tests pass)
//
// The program exits 1 if any CHECK fails, so ctest goes red. Run under
// valgrind or with -fsanitize=address to catch leaks.
#include <iostream>
#include "containers/DynamicArray.h"
#include "containers/LinkedList.h"
#include "containers/Queue.h"
#include "containers/MergeSort.h"
#include "core/Types.h"

static int g_checks = 0, g_failed = 0, g_todo = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; \
    cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)
#define TODO_TEST(name) do { ++g_todo; cout << "  TODO  " << name << "\n"; } while (0)

struct Record {
    int time;
    int order;
};

static bool earlier(const Record& a, const Record& b) {
    return a.time < b.time;
}

static bool smaller(const int& a, const int& b) {
    return a < b;
}

static void testDynamicArray() {
    DynamicArray<int> a;
    CHECK(a.size() == 0 && a.isEmpty());
    CHECK(!a.pop_back());

    for (int i = 0; i < 10000; i++) {
        a.push_back(i * 3);
    }
    bool ok = true;
    for (int i = 0; i < 10000; i++) {
        if (a[i] != i * 3) {
            ok = false;
        }
    }
    CHECK(a.size() == 10000 && ok);

    // copy is separate from the original
    DynamicArray<int> b = a;
    b[0] = -1;
    CHECK(a[0] == 0 && b[0] == -1);

    // assignment makes a separate copy; self-assignment is safe
    DynamicArray<int> c;
    c.push_back(7);
    c = a;
    c[1] = -5;
    CHECK(c.size() == 10000 && a[1] == 3);
    DynamicArray<int>& same = c;
    c = same;
    CHECK(c.size() == 10000 && c[1] == -5);

    // pushing an element of the same array while it grows
    DynamicArray<int> d;
    d.push_back(42);
    for (int i = 0; i < 20; i++) {
        d.push_back(d[0]);
    }
    CHECK(d.size() == 21 && d[20] == 42);

    // pop_back
    CHECK(d.pop_back());
    CHECK(d.size() == 20);

    // reserve: no reallocation for the reserved number of pushes
    DynamicArray<int> r;
    r.reserve(100);
    int capBefore = r.capacity();
    for (int i = 0; i < 100; i++) {
        r.push_back(i);
    }
    CHECK(capBefore >= 100 && r.capacity() == capBefore && r.size() == 100);
    DynamicArray<int> r2(50);
    CHECK(r2.size() == 0 && r2.capacity() >= 50);

    // assign
    r.assign(5, -1);
    CHECK(r.size() == 5 && r[0] == -1 && r[4] == -1);

    // const access
    const DynamicArray<int>& cr = r;
    CHECK(cr[2] == -1 && cr.size() == 5);

    // clear
    c.clear();
    CHECK(c.size() == 0 && c.isEmpty());

    // works with strings
    DynamicArray<Str> s;
    s.push_back("alpha");
    s.push_back("beta");
    CHECK(s[1] == "beta");

    // array of lists (adjacency) copies each list
    DynamicArray<LinkedList<int>> lists;
    for (int i = 0; i < 9; i++) {
        LinkedList<int> one;
        one.pushBack(i);
        lists.push_back(one);
    }
    CHECK(lists.size() == 9 && lists[8].first()->value == 8 && lists[0].size() == 1);
}

static void testLinkedList() {
    LinkedList<int> list;
    CHECK(list.isEmpty());
    CHECK(list.first() == nullptr);
    int v = 0;
    CHECK(!list.popFront(v));

    list.pushBack(1);
    list.pushBack(2);
    list.pushBack(3);
    list.pushFront(0);
    CHECK(list.size() == 4 && list.first()->value == 0);

    // iterate gives 0 1 2 3
    int expected = 0;
    bool ok = true;
    for (const LinkedList<int>::Node* n = list.first(); n != nullptr; n = n->next) {
        if (n->value != expected) {
            ok = false;
        }
        expected++;
    }
    CHECK(ok && expected == 4);

    // copy is separate
    LinkedList<int> copy = list;
    copy.popFront(v);
    CHECK(list.size() == 4 && copy.size() == 3);

    LinkedList<int> other;
    other.pushBack(9);
    other = list;
    CHECK(other.size() == 4 && other.first()->value == 0);
    LinkedList<int>& sameList = other;
    other = sameList;
    CHECK(other.size() == 4);

    CHECK(list.popFront(v) && v == 0);
    list.popFront(v);
    list.popFront(v);
    CHECK(list.popFront(v) && v == 3);
    CHECK(list.isEmpty() && list.first() == nullptr);

    // usable again after being emptied (tail was reset)
    list.pushBack(5);
    list.pushBack(6);
    CHECK(list.size() == 2 && list.first()->value == 5 && list.first()->next->value == 6);

    list.clear();
    CHECK(list.isEmpty());
}

static void testQueue() {
    Queue<int> q;
    int v = 0;
    CHECK(q.isEmpty());
    CHECK(!q.dequeue(v));
    CHECK(!q.front(v));

    for (int i = 1; i <= 100; i++) {
        q.enqueue(i);
    }
    CHECK(q.size() == 100);
    CHECK(q.front(v) && v == 1);
    CHECK(q.size() == 100);   // front does not remove

    bool ok = true;
    for (int i = 1; i <= 100; i++) {
        int f = 0;
        q.front(f);
        if (!q.dequeue(v) || v != i || f != i) {
            ok = false;
        }
    }
    CHECK(ok && q.isEmpty());

    // copy is separate
    q.enqueue(4);
    q.enqueue(7);
    Queue<int> other = q;
    other.dequeue(v);
    CHECK(q.size() == 2 && other.front(v) && v == 7);

    // first() walks front to back
    CHECK(q.first() != nullptr && q.first()->value == 4 && q.first()->next->value == 7);

    q.clear();
    CHECK(q.isEmpty() && q.first() == nullptr);
}

static void testMergeSort() {
    DynamicArray<int> nums;
    int values[10] = { 9, 4, 7, 1, 8, 2, 2, 6, 0, 5 };
    for (int i = 0; i < 10; i++) {
        nums.push_back(values[i]);
    }
    mergeSort(nums, smaller);
    bool ok = true;
    for (int i = 1; i < nums.size(); i++) {
        if (nums[i - 1] > nums[i]) {
            ok = false;
        }
    }
    CHECK(ok && nums.size() == 10 && nums[0] == 0 && nums[9] == 9);

    // equal keys keep their original order
    DynamicArray<Record> records;
    int times[8] = { 30, 10, 20, 10, 30, 10, 20, 0 };
    for (int i = 0; i < 8; i++) {
        Record r;
        r.time = times[i];
        r.order = i;
        records.push_back(r);
    }
    mergeSort(records, earlier);
    CHECK(records[0].time == 0 && records[7].time == 30);
    CHECK(records[1].order == 1 && records[2].order == 3 && records[3].order == 5);
    CHECK(records[4].order == 2 && records[5].order == 6);
    CHECK(records[6].order == 0 && records[7].order == 4);

    // empty array and one element
    DynamicArray<int> empty;
    mergeSort(empty, smaller);
    CHECK(empty.size() == 0);
    DynamicArray<int> one;
    one.push_back(3);
    mergeSort(one, smaller);
    CHECK(one.size() == 1 && one[0] == 3);

    // larger reversed input
    DynamicArray<int> big;
    for (int i = 1000; i > 0; i--) {
        big.push_back(i);
    }
    mergeSort(big, smaller);
    ok = true;
    for (int i = 0; i < 1000; i++) {
        if (big[i] != i + 1) {
            ok = false;
        }
    }
    CHECK(ok);
}

static void testTypesRoundTrip() {
    bool ok = true;
    LocationType lt;
    for (int i = 0; i < 6; i++) {
        if (!fromString(toString((LocationType)i), lt) || (int)lt != i) ok = false;
    }
    Capability cap;
    for (int i = 0; i < 5; i++) {
        if (!fromString(toString((Capability)i), cap) || (int)cap != i) ok = false;
    }
    Category cat;
    for (int i = 0; i < 4; i++) {
        if (!fromString(toString((Category)i), cat) || (int)cat != i) ok = false;
    }
    RobotState rs;
    for (int i = 0; i < 6; i++) {
        if (!fromString(toString((RobotState)i), rs) || (int)rs != i) ok = false;
    }
    RequestState qs;
    for (int i = 0; i < 5; i++) {
        if (!fromString(toString((RequestState)i), qs) || (int)qs != i) ok = false;
    }
    EventType ev;
    for (int i = 0; i < 7; i++) {
        if (!fromString(toString((EventType)i), ev) || (int)ev != i) ok = false;
    }
    ConnStatus cs;
    for (int i = 0; i < 2; i++) {
        if (!fromString(toString((ConnStatus)i), cs) || (int)cs != i) ok = false;
    }
    CHECK(ok);

    CHECK(toString(LocationType::CHARGING) == "CHARGING");
    CHECK(toString(EventType::SET_URGENCY) == "SET_URGENCY");
    CHECK(toString(RequestState::IN_PROGRESS) == "IN_PROGRESS");
    CHECK(toString(FailReason::NOT_SERVED) == "not_served");
    CHECK(toString(FailReason::UNREACHABLE) == "unreachable");
    CHECK(fromString("GENERAL", cap) && cap == Capability::GENERAL);

    // bad spellings
    CHECK(!fromString("HOUSE", lt));
    CHECK(!fromString("PILOT", cap));
    CHECK(!fromString("GENERAL", cat));
    CHECK(!fromString("junction", lt));
    CHECK(!fromString("", ev));
}

int main() {
    cout << "test_containers\n";
    testDynamicArray();
    testLinkedList();
    testQueue();
    testMergeSort();
    testTypesRoundTrip();
    cout << g_checks << " checks, " << g_failed << " failed, " << g_todo << " tests still TODO\n";
    return g_failed == 0 ? 0 : 1;
}
