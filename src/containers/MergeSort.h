// File:       src/containers/MergeSort.h
// Owner:      Farwa
// Purpose:    Stable merge sort on a DynamicArray. The loader sorts events and
//             requests by time; stability keeps same-time records in FILE
//             order without an extra sequence field (Report 5.7).
// Depends on: DynamicArray
// Used by:    ScenarioLoader (events, requests by time; robot indices by ID)
// Milestone:  M1
//
// Usage:
//     bool earlier(const Event& a, const Event& b) { return a.time < b.time; }
//     mergeSort(world.events, earlier);
// `less(a, b)` must return true only when a must come STRICTLY before b.
// When less(a,b) and less(b,a) are both false, a and b keep their order.
#ifndef RESQGRID_MERGE_SORT_H
#define RESQGRID_MERGE_SORT_H

#include "DynamicArray.h"

// Merge the sorted runs a[lo..mid) and a[mid..hi) into tmp, then copy back.
// The right item goes first only when it is strictly smaller (keeps it stable).
template <typename T, typename Less>
void mergeRuns(DynamicArray<T>& a, DynamicArray<T>& tmp, int lo, int mid, int hi, Less less) {
    int i = lo;
    int j = mid;
    int k = lo;

    while (i < mid && j < hi) {
        if (less(a[j], a[i])) {
            tmp[k] = a[j];
            j++;
        } else {
            tmp[k] = a[i];
            i++;
        }
        k++;
    }
    while (i < mid) {
        tmp[k] = a[i];
        i++;
        k++;
    }
    while (j < hi) {
        tmp[k] = a[j];
        j++;
        k++;
    }

    for (k = lo; k < hi; k++) {
        a[k] = tmp[k];
    }
}

// Recursive merge sort of a[lo..hi).
template <typename T, typename Less>
void mergeSortRange(DynamicArray<T>& a, DynamicArray<T>& tmp, int lo, int hi, Less less) {
    if (hi - lo < 2) {
        return;
    }
    int mid = (lo + hi) / 2;
    mergeSortRange(a, tmp, lo, mid, less);
    mergeSortRange(a, tmp, mid, hi, less);
    mergeRuns(a, tmp, lo, mid, hi, less);
}

template <typename T, typename Less>
void mergeSort(DynamicArray<T>& a, Less less) {
    if (a.size() < 2) {
        return;
    }
    DynamicArray<T> tmp(a);     // same size as a, allocated once
    mergeSortRange(a, tmp, 0, a.size(), less);
}

#endif // RESQGRID_MERGE_SORT_H
