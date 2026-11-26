// File:       src/containers/IndexedMaxHeap.h
// Owner:      Sara
// Purpose:    Indexed binary max-heap of request indices: always gives the most
//             important PENDING request and supports change-key for
//             SET_URGENCY (Report 5.6).
// Depends on: DynamicArray
// Used by:    Coordinator (owns the pending heap), EventProcessor (SET_URGENCY
//             change-key), Rerouter / EventProcessor (put released requests back)
// Milestone:  M1
//
// Items are request indices 0..capacity-1. The caller computes the key with
// Request::priorityKey() = A * urgency - report_time, and passes the request's
// ID as `tieId`. Order: LARGER key first; equal keys -> SMALLER tieId first.
//
// Arrays:
//   heap[i]    = item stored at heap position i   (size = number in heap)
//   pos[item]  = heap position of item, or -1 if not in the heap
//   key[item], tie[item] = current priority of item
// Invariant: for every item in the heap, heap[pos[item]] == item, and every
// parent ranks at least as high as its children. Update pos[] on EVERY swap.
//
// Header-only like the template containers (not a template itself, so that it
// can be tested without Request).
#ifndef RESQGRID_INDEXED_MAX_HEAP_H
#define RESQGRID_INDEXED_MAX_HEAP_H

#include "DynamicArray.h"

class IndexedMaxHeap {
public:
    IndexedMaxHeap() : count(0) {}

    // Prepare space for `capacity` items (= number of requests). Called once
    // after loading. Every item starts "not in the heap" (pos = -1).
    void init(int capacity) {
        if (capacity < 0) {
            capacity = 0;
        }
        // Drop anything from a previous run FIRST: clear() uses the OLD pos[]
        // array, which still matches the old heap contents.
        heap.clear();
        count = 0;

        // pos[item] = -1 means "this item is not in the heap".
        pos.assign(capacity, -1);
        // key[] and tie[] are only meaningful for items that are in the heap.
        key.assign(capacity, 0);
        tie.assign(capacity, 0);
        // The heap itself starts empty and grows as items are inserted.
        heap.reserve(capacity);
    }

    // Put `item` at the end of the heap and let it rise to its correct place.
    // Returns false if the item is already in the heap (or out of range).
    bool insert(int item, long long itemKey, int tieId) {
        if (item < 0 || item >= pos.size()) {
            return false;           // not a valid request index
        }
        if (pos[item] != -1) {
            return false;           // already waiting in the heap
        }

        key[item] = itemKey;
        tie[item] = tieId;

        // Append at the end: its temporary position is the current size.
        heap.push_back(item);
        pos[item] = count;
        ++count;

        // Restore the heap property by moving it up towards the root.
        siftUp(count - 1);
        return true;
    }

    // Remove and return the highest-priority item (the root), or -1 if empty.
    int extractMax() {
        if (count == 0) {
            return -1;
        }

        int root = heap[0];
        pos[root] = -1;             // the root is leaving the heap

        // Take the last item out and put it at the root, then let it sink.
        int last = heap[count - 1];
        heap.pop_back();
        --count;

        if (count > 0) {
            heap[0] = last;
            pos[last] = 0;
            siftDown(0);
        }
        return root;
    }

    // The highest-priority item without removing it, or -1 if empty. O(1).
    int peekMax() const {
        if (count == 0) {
            return -1;
        }
        return heap[0];
    }

    // SET_URGENCY: change an item's key and repair the heap in place.
    // If the key went up it rises, otherwise it sinks. False if not in the heap.
    bool changeKey(int item, long long newKey) {
        if (item < 0 || item >= pos.size()) {
            return false;
        }
        if (pos[item] == -1) {
            return false;           // not waiting in the heap
        }

        bool wentUp = newKey > key[item];
        key[item] = newKey;

        int position = pos[item];
        if (wentUp) {
            siftUp(position);       // may now outrank its parent
        } else {
            siftDown(position);     // may now be smaller than its children
        }
        return true;
    }

    // True if the item is currently waiting in the heap.
    bool contains(int item) const {
        return item >= 0 && item < pos.size() && pos[item] != -1;
    }

    // Empty the heap. Items that were inside are marked "not in the heap".
    void clear() {
        for (int i = 0; i < count; ++i) {
            pos[heap[i]] = -1;
        }
        heap.clear();
        count = 0;
    }

    int  size() const    { return count; }
    bool isEmpty() const { return count == 0; }

    // Read-only access for the step-mode pending list (heap order, not sorted).
    int itemAt(int heapPos) const { return heap[heapPos]; }

private:
    // Our ordering rule: larger key wins; if keys are equal, the smaller
    // request ID (tieId) wins. Returns true when `a` belongs above `b`.
    bool higher(int a, int b) const {
        if (key[a] != key[b]) {
            return key[a] > key[b];
        }
        return tie[a] < tie[b];
    }

    // Swap two heap positions AND keep pos[] in sync (the core invariant).
    void swapAt(int i, int j) {
        int tmp = heap[i];
        heap[i] = heap[j];
        heap[j] = tmp;

        pos[heap[i]] = i;
        pos[heap[j]] = j;
    }

    // Move the item at position i up while it outranks its parent.
    void siftUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (!higher(heap[i], heap[parent])) {
                break;
            }
            swapAt(i, parent);
            i = parent;
        }
    }

    // Move the item at position i down while one of its children outranks it.
    void siftDown(int i) {
        while (true) {
            int left  = 2 * i + 1;
            int right = 2 * i + 2;
            int best  = i;

            if (left < count && higher(heap[left], heap[best])) {
                best = left;
            }
            if (right < count && higher(heap[right], heap[best])) {
                best = right;
            }
            if (best == i) {
                break;              // already higher than both children
            }
            swapAt(i, best);
            i = best;
        }
    }

    DynamicArray<int>       heap;
    DynamicArray<int>       pos;
    DynamicArray<long long> key;
    DynamicArray<int>       tie;
    int count;
};

#endif // RESQGRID_INDEXED_MAX_HEAP_H
