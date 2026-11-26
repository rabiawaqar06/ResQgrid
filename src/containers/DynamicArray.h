// File:       src/containers/DynamicArray.h
// Owner:      Farwa
// Purpose:    Growable array (our replacement for std::vector). Main storage
//             for locations, connections, robots, requests and events; every
//             entity is referred to by its index 0..n-1 (Report 5.2).
// Depends on: nothing
// Used by:    almost every other file
// Milestone:  M1 (merge by 16 Nov, everyone needs it)
//
// Rules: push_back is O(1) amortised (capacity doubles when full); operator[]
// is O(1). The array owns its memory: copy constructor and assignment make a
// deep copy, the destructor frees it. No bounds check in operator[] (callers
// use valid indices); add an assert while debugging if you like.
// clear() keeps the buffer, so refilling the array does not allocate again.
#ifndef RESQGRID_DYNAMIC_ARRAY_H
#define RESQGRID_DYNAMIC_ARRAY_H

template <typename T>
class DynamicArray {
public:
    DynamicArray() : data(nullptr), count(0), cap(0) {}

    explicit DynamicArray(int initialCapacity) : data(nullptr), count(0), cap(0) {
        reserve(initialCapacity);
    }

    DynamicArray(const DynamicArray& other) : data(nullptr), count(0), cap(0) {
        reserve(other.count);
        for (int i = 0; i < other.count; i++) {
            push_back(other.data[i]);
        }
    }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this == &other) {
            return *this;
        }
        clear();
        reserve(other.count);
        for (int i = 0; i < other.count; i++) {
            push_back(other.data[i]);
        }
        return *this;
    }

    ~DynamicArray() {
        delete[] data;
    }

    void push_back(const T& value) {
        T copy = value;     // value may live inside this array, so copy it before growing
        if (count == cap) {
            if (cap == 0) {
                grow(1);
            } else {
                grow(cap * 2);
            }
        }
        data[count] = copy;
        count++;
    }

    bool pop_back() {
        if (count == 0) {
            return false;
        }
        count--;
        return true;
    }

    T& operator[](int index) {
        return data[index];
    }
    const T& operator[](int index) const {
        return data[index];
    }

    void reserve(int newCap) {
        if (newCap > cap) {
            grow(newCap);
        }
    }

    void assign(int n, const T& value) {
        T copy = value;
        clear();
        reserve(n);
        for (int i = 0; i < n; i++) {
            push_back(copy);
        }
    }

    void clear() {
        count = 0;
    }

    int  size() const     { return count; }
    int  capacity() const { return cap; }
    bool isEmpty() const  { return count == 0; }

private:
    void grow(int newCap) {
        T* bigger = new T[newCap];
        for (int i = 0; i < count; i++) {
            bigger[i] = data[i];
        }
        delete[] data;
        data = bigger;
        cap = newCap;
    }

    T*  data;
    int count;  // number of elements in use
    int cap;    // number of slots allocated
};

#endif // RESQGRID_DYNAMIC_ARRAY_H
