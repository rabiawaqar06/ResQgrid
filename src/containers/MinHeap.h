// File:       src/containers/MinHeap.h
// Owner:      Rabia
// Purpose:    Array-based binary min-heap. Dijkstra stores (distance, location)
//             entries in it and uses LAZY DELETION: a shorter distance pushes
//             a new entry instead of updating the old one; outdated entries are
//             skipped when popped (Report 5.5).
// Depends on: DynamicArray
// Used by:    Router
// Milestone:  M1
//
// T must support `a < b` (Router defines DistEntry with operator<).
// push / popMin O(log n); top / isEmpty O(1). Root is at index 0; children of
// i are 2i+1 and 2i+2; parent is (i-1)/2.
#ifndef RESQGRID_MIN_HEAP_H
#define RESQGRID_MIN_HEAP_H

#include "DynamicArray.h"

template <typename T>
class MinHeap {
public:
    void push(const T& value) {
        items.push_back(value);
        siftUp(items.size() - 1);
    }

    // Remove the smallest element into `out`. False if the heap is empty.
    bool popMin(T& out) {
        if (items.size() == 0) {
            return false;
        }
        out = items[0];
        items[0] = items[items.size() - 1];   // last element goes to the root
        items.pop_back();
        if (items.size() > 0) {
            siftDown(0);
        }
        return true;
    }

    bool top(T& out) const {
        if (items.size() == 0) {
            return false;
        }
        out = items[0];
        return true;
    }

    void clear() { items.clear(); }

    bool isEmpty() const { return items.size() == 0; }

    int size() const { return items.size(); }

private:
    // move item i up while it is smaller than its parent
    void siftUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (items[i] < items[parent]) {
                T tmp = items[i];
                items[i] = items[parent];
                items[parent] = tmp;
                i = parent;
            } else {
                break;
            }
        }
    }

    // move item i down while a child is smaller than it
    void siftDown(int i) {
        int n = items.size();
        while (true) {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            int smallest = i;
            if (left < n && items[left] < items[smallest]) {
                smallest = left;
            }
            if (right < n && items[right] < items[smallest]) {
                smallest = right;
            }
            if (smallest == i) {
                break;
            }
            T tmp = items[i];
            items[i] = items[smallest];
            items[smallest] = tmp;
            i = smallest;
        }
    }

    DynamicArray<T> items;
};

#endif // RESQGRID_MIN_HEAP_H
