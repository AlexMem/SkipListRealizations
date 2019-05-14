#include <iostream>
#include <random>
#include "atomic_markable_reference.h"
#include "hazard_domain.h"

template <class T> class ConcurrentSkipList {
    template <class E> class Node {
    public:
        E value;
        unsigned int level;
        AtomicMarkableReference<Node<E>>* nexts;

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

// FIELDS
private:
    double P;
    unsigned int maxHeight;

    std::random_device randomDevice;

    HazardDomain<Node<T>>* hazardDomain;

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
        hazardDomain = new HazardDomain<Node<T>>(2*maxHeight+4, maxNumOfThreads);
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
        delete hazardDomain;
    }

// PUBLIC METHODS
public:
    bool contains(T value) {
        int botLvl = 0;
        bool mark;
        int hzCellIndex = hazardDomain->acquireCell();

        Node<T>* pred = hazardDomain->protect(head, hzCellIndex, 0);
        Node<T>* curr;
        Node<T>* succ;

        for (int lvl = maxHeight-1; lvl >= botLvl; --lvl) {
            curr = hazardDomain->protect(pred->nexts[lvl].getRef(), hzCellIndex, 1);
            while(true) {
                succ = hazardDomain->protect(curr->nexts[lvl].getRefAndMark(mark), hzCellIndex, 2);
                while(curr != tail && mark) {
                    curr = hazardDomain->protect(curr->nexts[lvl].getRef(), hzCellIndex, 1);
                    succ = hazardDomain->protect(curr->nexts[lvl].getRefAndMark(mark), hzCellIndex, 2);
                }
                if(compare(curr, value) < 0) {
                    pred = hazardDomain->protect(curr, hzCellIndex, 0);
                    curr = hazardDomain->protect(succ, hzCellIndex, 1);
                } else {
                    break;
                }
            }
        }

        bool result = (compare(curr, value) == 0);
        hazardDomain->releaseCell(hzCellIndex);
        return result;
    }

    bool find(T value, Node<T>** preds, Node<T>** succs, int hzCellIndex) {
        int botLvl = 0;
        bool mark;

        Node<T>* pred;
        Node<T>* curr;
        Node<T>* succ;

    retry:
        while (true) {
            pred = hazardDomain->protect(head, hzCellIndex, 0);
            for (int lvl = maxHeight-1; lvl >= botLvl; --lvl) {
                curr = hazardDomain->protectWithValidation(pred->nexts[lvl], hzCellIndex, 1);
                // linearization point if lvl == 0
                while (true) {
                    succ = hazardDomain->protectWithValidation(curr->nexts[lvl], mark, hzCellIndex, 2);
                    while (curr != tail && mark){
                        if(!pred->nexts[lvl].CAS(curr, succ, false, false)) {
                            goto retry;
                        }
                        curr = hazardDomain->protectWithValidation(pred->nexts[lvl], hzCellIndex, 1);
                        // linearization point if lvl == 0
                        succ = hazardDomain->protectWithValidation(curr->nexts[lvl], mark, hzCellIndex, 2);
                    }

                    if(compare(curr, value) < 0) {
                        pred = hazardDomain->protect(curr, hzCellIndex, 0);
                        curr = hazardDomain->protect(succ, hzCellIndex, 1);
                    } else {
                        break;
                    }
                }

                preds[lvl] = hazardDomain->protect(pred, hzCellIndex, 2 * lvl + 4);
                succs[lvl] = hazardDomain->protect(curr, hzCellIndex, 2 * lvl + 5);
            }

            return compare(curr, value) == 0;
        }
    }

    bool add(T value) {
        unsigned int topLvl = getRandomLevel();
        int botLvl = 0;
        int hzCellIndex = hazardDomain->acquireCell();
        Node<T>** preds = new Node<T>*[maxHeight];
        Node<T>** succs = new Node<T>*[maxHeight];

        while(true) {
            if(find(value, preds, succs, hzCellIndex)){
                delete[] preds;
                delete[] succs;
                hazardDomain->releaseCell(hzCellIndex);
                return false;
            }

            Node<T>* newNode = new Node<T>(value, topLvl);
            for (int lvl = botLvl; lvl <= topLvl; ++lvl) {
                newNode->nexts[lvl].setVal(succs[lvl], false);
            }
            Node<T>* pred = preds[botLvl];
            Node<T>* succ = succs[botLvl];

            hazardDomain->protect(newNode, hzCellIndex, 3);
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
                    find(value, preds, succs, hzCellIndex);
                }
            }

            delete[] preds;
            delete[] succs;
            hazardDomain->releaseCell(hzCellIndex);
            return true;
        }
    }

    bool remove(T value) {
        bool mark;
        int botLvl = 0;
        int hzCellIndex = hazardDomain->acquireCell();
        Node<T>** preds = new Node<T>*[maxHeight];
        Node<T>** succs = new Node<T>*[maxHeight];
        Node<T>* succ;

        while(true) {
            if(!find(value, preds, succs, hzCellIndex)) {
                delete[] preds;
                delete[] succs;
                hazardDomain->releaseCell(hzCellIndex);
                return false;
            }

            Node<T>* toRemove = hazardDomain->protect(succs[botLvl], hzCellIndex, 3);
            for (int lvl = toRemove->level; lvl >= botLvl+1; --lvl) {
                succ = hazardDomain->protect(toRemove->nexts[lvl].getRefAndMark(mark), hzCellIndex, 2);
                while(!mark) {
                    toRemove->nexts[lvl].CAS(succ, succ, false, true);
                    succ = hazardDomain->protect(toRemove->nexts[lvl].getRefAndMark(mark), hzCellIndex, 2);
                }
            }

            succ = hazardDomain->protect(toRemove->nexts[botLvl].getRefAndMark(mark), hzCellIndex, 2);
            while(true) {
                bool markedIt = toRemove->nexts[botLvl].CAS(succ, succ, false, true);
                // linearization point if markedIf == true
                succ = hazardDomain->protect(succs[botLvl]->nexts[botLvl].getRefAndMark(mark), hzCellIndex, 2);
                if(markedIt) {
                    find(value, preds, succs, hzCellIndex);
                    hazardDomain->deletePtr(toRemove, hzCellIndex);
                    delete[] preds;
                    delete[] succs;
                    hazardDomain->releaseCell(hzCellIndex);
                    return true;
                } else {
                    if(mark) {
                        delete[] preds;
                        delete[] succs;
                        hazardDomain->releaseCell(hzCellIndex);
                        return false;
                    }
                }
            }
        }
    }

    void print() {
        int i = 0;
        for (Node<T> *p = head; p!=tail; p=p->nexts[0].getRef()) {
            if(p == head) {
                std::cout << i++ << ".\th [" << p->level << "]\t->\t";
            } else {
                std::cout << i++ << ".\t" << p->value << " [" << p->level << "]\t->\t";
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
