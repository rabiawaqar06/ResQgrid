// File:       src/containers/LinkedList.h
// Owner:      Farwa
// Purpose:    Singly linked list with head and tail pointers. Used for
//             adjacency lists (Report 5.4), hash-map buckets (Report 5.3) and
//             as the storage behind Queue (Report 5.8).
// Depends on: nothing
// Used by:    Queue, HashMap, Environment (adjacency), anything that iterates
// Milestone:  M1 (merge by 16 Nov, everyone needs it)
//
// How to iterate (no STL iterators):
//     for (const LinkedList<T>::Node* n = list.first(); n != nullptr; n = n->next)
//         use(n->value);
#ifndef RESQGRID_LINKED_LIST_H
#define RESQGRID_LINKED_LIST_H

template <typename T>
class LinkedList {
public:
    struct Node {
        T     value;
        Node* next;
        explicit Node(const T& v) : value(v), next(nullptr) {}
    };

    LinkedList() : head(nullptr), tail(nullptr), count(0) {}

    LinkedList(const LinkedList& other) : head(nullptr), tail(nullptr), count(0) {
        Node* current = other.head;
        while (current != nullptr) {
            pushBack(current->value);
            current = current->next;
        }
    }

    LinkedList& operator=(const LinkedList& other) {
        if (this == &other) {
            return *this;
        }
        clear();
        Node* current = other.head;
        while (current != nullptr) {
            pushBack(current->value);
            current = current->next;
        }
        return *this;
    }

    ~LinkedList() {
        clear();
    }

    void pushFront(const T& value) {
        Node* node = new Node(value);
        node->next = head;
        head = node;
        if (tail == nullptr) {
            tail = node;
        }
        count++;
    }

    void pushBack(const T& value) {
        Node* node = new Node(value);
        if (head == nullptr) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
        count++;
    }

    bool popFront(T& out) {
        if (head == nullptr) {
            return false;
        }
        Node* old = head;
        out = old->value;
        head = head->next;
        if (head == nullptr) {
            tail = nullptr;
        }
        delete old;
        count--;
        return true;
    }

    void clear() {
        while (head != nullptr) {
            Node* nextNode = head->next;
            delete head;
            head = nextNode;
        }
        tail = nullptr;
        count = 0;
    }

    // First node for iteration (nullptr when empty). Front value: first()->value.
    Node*       first()       { return head; }
    const Node* first() const { return head; }

    int  size() const    { return count; }
    bool isEmpty() const { return count == 0; }

private:
    Node* head;
    Node* tail;
    int   count;
};

#endif // RESQGRID_LINKED_LIST_H
