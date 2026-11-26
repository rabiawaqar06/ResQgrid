// File:       src/containers/HashMap.h
// Owner:      Rabia
// Purpose:    Map from an external integer ID (as written in the file) to an
//             internal index. Separate chaining: an array of LinkedList
//             buckets, h(k) = k mod m, m a prime about twice the number of
//             entries (Report 5.3). A failed insert is how the loader detects
//             duplicate IDs.
// Depends on: LinkedList, DynamicArray
// Used by:    World (the ID -> index maps), ScenarioLoader, EventProcessor
// Milestone:  M1
//
// Keys are int IDs (positive in valid files, but negative keys must not crash:
// use a non-negative modulo). Insert/find O(1) average, O(n) worst case.
#ifndef RESQGRID_HASH_MAP_H
#define RESQGRID_HASH_MAP_H

#include "DynamicArray.h"
#include "LinkedList.h"

template <typename V>
class HashMap {
public:
    struct Entry {
        int key;
        V   value;
    };

    HashMap() : entryCount(0) {}

    // Choose m = smallest prime >= 2 * expectedCount (at least 2) and make m
    // empty buckets. Call before the first insert.
    void init(int expectedCount) {
        int m = 2 * expectedCount;
        if (m < 2) {
            m = 2;
        }
        // go up from m until we hit a prime
        while (true) {
            bool prime = true;
            for (int d = 2; d * d <= m; d++) {
                if (m % d == 0) {
                    prime = false;
                    break;
                }
            }
            if (prime) {
                break;
            }
            m++;
        }
        buckets.assign(m, LinkedList<Entry>());
        entryCount = 0;
    }

    // Add {key, value}. Returns false if the key is already there (no overwrite).
    bool insert(int key, const V& value) {
        if (buckets.size() == 0) {
            init(1);    // init() was not called: use the smallest table
        }
        if (contains(key)) {
            return false;
        }
        Entry e;
        e.key = key;
        e.value = value;
        buckets[bucketOf(key)].pushBack(e);
        entryCount++;
        return true;
    }

    // Copy the value of `key` into `out`. False if the key is not there.
    bool find(int key, V& out) const {
        if (buckets.size() == 0) {
            return false;
        }
        const typename LinkedList<Entry>::Node* n = buckets[bucketOf(key)].first();
        while (n != nullptr) {
            if (n->value.key == key) {
                out = n->value.value;
                return true;
            }
            n = n->next;
        }
        return false;
    }

    bool contains(int key) const {
        V dummy = V();
        return find(key, dummy);
    }

    // Empty every bucket but keep the bucket array.
    void clear() {
        for (int i = 0; i < buckets.size(); i++) {
            buckets[i].clear();
        }
        entryCount = 0;
    }

    int size() const        { return entryCount; }
    int bucketCount() const { return buckets.size(); }

private:
    // h(k) = k mod m, kept in 0..m-1 even for negative keys
    int bucketOf(int key) const {
        int m = buckets.size();
        int h = key % m;
        if (h < 0) {
            h += m;
        }
        return h;
    }

    DynamicArray<LinkedList<Entry>> buckets;
    int entryCount;
};

#endif // RESQGRID_HASH_MAP_H
