#include <iostream>
#include <random>
#include "atomic_markable_reference.h"

template <class T> class ConcurrentSkipList {
    template <class E> class Node {
    public:
        E value;
        unsigned int level;
        AtomicMarkableReference<Node<E>>* nexts;

        std::atomic<Node<E>*> nextFree{nullptr};

        explicit Node(unsigned int lvl) {
            nexts = new AtomicMarkableReference<Node<E>>[lvl+1];
            level = lvl;
        }

        Node(E value, unsigned int lvl) {
            this->value = value;
            nexts = new AtomicMarkableReference<Node<E>>[lvl+1];
            level = lvl;
        }

        ~Node() {
            delete[] nexts;
        }
    };

    class FreeList {
        std::atomic<Node<T>*> freeListHead{nullptr};
        std::atomic<Node<T>*> freeListTail{nullptr};
    public:
        FreeList() {
            Node<T>* headNode = new Node<T>(0);
            freeListHead.store(headNode);
            freeListTail.store(headNode);
        }

        void add(Node<T>* newNode) {
            Node<T>* currentTail;
            do {
                currentTail = freeListTail.load();
            } while (!freeListTail.compare_exchange_strong(currentTail, newNode));
            currentTail->nextFree.store(newNode);
        }

        ~FreeList() {
            for (Node<T>* p = freeListHead.load(); p != nullptr; p = freeListHead.load()) {
                freeListHead.store(freeListHead.load()->nextFree.load());
                delete p;
            }
        }

    };

// FIELDS
private:
    double P;
    unsigned int maxHeight;

    std::random_device randomDevice;

    FreeList freeList;

    Node<T>* head;
    Node<T>* tail;

// CONSTRUCTORS
public:
    explicit ConcurrentSkipList(unsigned int maxHeight = 20, unsigned int maxNumOfThreads = 8, double P = 0.7) {
        this->maxHeight = maxHeight;
        this->P = P;
        tail = new Node<T>(maxHeight-1);
        head = new Node<T>(maxHeight-1);
        for(int i = 0; i < maxHeight; ++i) {
            head->nexts[i].setVal(tail, false);
        }
    }

//DESTRUCTOR
    ~ConcurrentSkipList() {
        Node<T> *toDel;
        for (Node<T> *p = head; p!=tail;) {
            toDel = p;
            p=p->nexts[0].getRef();
            delete toDel;
        }
        delete tail;
    }

// PUBLIC METHODS
public:
    bool contains(T value) {
        int botLvl = 0;
        bool mark;

        Node<T>* pred = head;
        Node<T>* curr;
        Node<T>* succ;

        for (int lvl = maxHeight-1; lvl >= botLvl; --lvl) {
            curr = pred->nexts[lvl].getRef();
            while(true) {
                succ = curr->nexts[lvl].getRefAndMark(mark);
                while(curr != tail && mark) {
                    curr = curr->nexts[lvl].getRef();
                    succ = curr->nexts[lvl].getRefAndMark(mark);
                }
                if(compare(curr, value) < 0) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }

        return compare(curr, value) == 0;
    }

    bool find(T value, Node<T>** preds, Node<T>** succs) {
        int botLvl = 0;
        bool mark;

        Node<T>* pred;
        Node<T>* curr;
        Node<T>* succ;

    retry:
        while (true) {
            pred = head;
            for (int lvl = maxHeight-1; lvl >= botLvl; --lvl) {
                curr = pred->nexts[lvl].getRef();
                // linearization point if lvl == 0
                while (true) {
                    succ = curr->nexts[lvl].getRefAndMark(mark);
                    while (curr != tail && mark){
                        if(!pred->nexts[lvl].CAS(curr, succ, false, false)) {
                            goto retry;
                        }
                        curr = pred->nexts[lvl].getRef();
                        // linearization point if lvl == 0
                        succ = curr->nexts[lvl].getRefAndMark(mark);
                    }

                    if(compare(curr, value) < 0) {
                        pred = curr;
                        curr = succ;
                    } else {
                        break;
                    }
                }

                preds[lvl] = pred;
                succs[lvl] = curr;
            }

            return compare(curr, value) == 0;
        }
    }

    bool add(T value) {
        unsigned int topLvl = getRandomLevel();
        int botLvl = 0;
        Node<T>** preds = new Node<T>*[maxHeight];
        Node<T>** succs = new Node<T>*[maxHeight];

        while(true) {
            if(find(value, preds, succs)){
                delete[] preds;
                delete[] succs;
                return false;
            }

            Node<T>* newNode = new Node<T>(value, topLvl);
            for (int lvl = botLvl; lvl <= topLvl; ++lvl) {
                newNode->nexts[lvl].setVal(succs[lvl], false);
            }
            Node<T>* pred = preds[botLvl];
            Node<T>* succ = succs[botLvl];

            if(!pred->nexts[botLvl].CAS(succ, newNode, false, false)) {
                delete newNode;
                continue;
            }
            // linearization point

            for (int lvl = botLvl+1; lvl <= topLvl; ++lvl) {
                while(true) {
                    pred = preds[lvl];
                    succ = succs[lvl];
                    if(pred->nexts[lvl].CAS(succ, newNode, false, false)) {
                        break;
                    }
                    find(value, preds, succs);
                }
            }

            delete[] preds;
            delete[] succs;
            return true;
        }
    }

    bool remove(T value) {
        bool markedIt;
        bool mark;
        int botLvl = 0;
        Node<T>** preds = new Node<T>*[maxHeight];
        Node<T>** succs = new Node<T>*[maxHeight];
        Node<T>* succ;

        while(true) {
            if(!find(value, preds, succs)) {
                delete[] preds;
                delete[] succs;
                return false;
            }

            Node<T>* toRemove = succs[botLvl];
            for (int lvl = toRemove->level; lvl >= botLvl+1; --lvl) {
                succ = toRemove->nexts[lvl].getRefAndMark(mark);
                while(!mark) {
                    toRemove->nexts[lvl].CAS(succ, succ, false, true);
                    succ = toRemove->nexts[lvl].getRefAndMark(mark);
                }
            }

            succ = toRemove->nexts[botLvl].getRefAndMark(mark);
            while(true) {
                markedIt = toRemove->nexts[botLvl].CAS(succ, succ, false, true);
                // linearization point if markedIf == true
                succ = succs[botLvl]->nexts[botLvl].getRefAndMark(mark);
                if(markedIt) {
                    find(value, preds, succs);
                    freeList.add(toRemove);
                    delete[] preds;
                    delete[] succs;
                    return true;
                } else {
                    if(mark) {
                        delete[] preds;
                        delete[] succs;
                        return false;
                    }
                }
            }
        }
    }

    void print() {
        int i = 0;
        int minLvl = maxHeight;
        int maxLvl = 0;
        for (Node<T> *p = head; p!=tail; p=p->nexts[0].getRef()) {
            if(p == head) {
                std::cout << i++ << ".\th [" << p->level << "]\t->\t";
            } else {
                std::cout << i++ << ".\t" << p->value << " [" << p->level << "]\t->\t";
                if(p->level < minLvl) {
                    minLvl = p->level;
                }
                if(p->level > maxLvl) {
                    maxLvl = p->level;
                }
            }
            for (int j = 0; j <= p->level; ++j) {
                if(p->nexts[j].getRef() != tail) {
                    std::cout << p->nexts[j].getRef()->value << "[" << p->nexts[j].getMark() << "]\t";
                } else {
                    std::cout << "n[0]\t";
                }
            }
            std::cout << std::endl;
        }
        std::cout << "minLvl: " << minLvl << std::endl;
        std::cout << "maxLvl: " << maxLvl << std::endl;
    }

    void checkForLockFree() {
        for(Node<T>* curr = head; curr != tail; curr = curr->nexts[0].getRef()) {
            std::cout << "Node " << curr->value << ":" << std::endl;
            checkNodeForLockFree(curr);
        }
    }

// PRIVATE METHODS
private:
    unsigned int getRandomLevel() {
        unsigned int lvl = 0;
        std::mt19937 mt(randomDevice());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        while (dist(mt) < P && lvl < maxHeight-1) {
            ++lvl;
        }
        return lvl;
    }

    int compare(Node<T>* node, T other) {
        if (node == tail) return 1;
        if (node->value < other) return -1;
        if (node->value == other) return 0;
        return 1;
    }

    void checkNodeForLockFree(Node<T>* node){
        std::cout << "\tValue: " << node->value << std::endl;
        for(int i = 0; i <= node->level; ++i) {
            std::cout << "\t" << i << ". is LF: " << node->nexts[i].is_lock_free() << std::endl;
        }
    }
};
