// File:       src/containers/Queue.h
// Owner:      Farwa
// Purpose:    FIFO queue built on LinkedList. A robot's planned route is a
//             Queue of connection indices: arriving at a location removes the
//             front element in O(1); rerouting replaces the whole queue
//             (Report 5.8).
// Depends on: LinkedList
// Used by:    Robot (route), Movement, Rerouter, Router (builds routes)
// Milestone:  M1
#ifndef RESQGRID_QUEUE_H
#define RESQGRID_QUEUE_H

#include "LinkedList.h"

template <typename T>
class Queue {
public:
    void enqueue(const T& value) {
        items.pushBack(value);
    }

    bool dequeue(T& out) {
        return items.popFront(out);
    }

    bool front(T& out) const {
        if (items.isEmpty()) {
            return false;
        }
        out = items.first()->value;
        return true;
    }

    void clear() {
        items.clear();
    }

    bool isEmpty() const { return items.isEmpty(); }
    int  size() const    { return items.size(); }

    // Read-only walk over the queue from front to back (Rerouter scans routes,
    // Router::routeToString prints them).
    const typename LinkedList<T>::Node* first() const { return items.first(); }

private:
    LinkedList<T> items;
};

#endif // RESQGRID_QUEUE_H
