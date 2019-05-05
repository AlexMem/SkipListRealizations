#ifndef CONCURRENTSKIPLIST_HAZARD_DOMAIN_H
#define CONCURRENTSKIPLIST_HAZARD_DOMAIN_H

#endif //CONCURRENTSKIPLIST_HAZARD_DOMAIN_H

#include <atomic>

using namespace std;

template<class T> class HazardDomain {
    template<class E> class HazardCell {
    public:
        // 0 - pred
        // 1 - curr
        // 2 - succ
        // 3 - [free]
        // 4 - newNode/toRemove
        // 5-44 - preds and succs
        const static int numOfRefs = 45;
        const static int numOfToDeleteRefs = 5;

        atomic<bool> isFree{true};
        atomic<E*>* refs;
        atomic<E*>* toDeleteRefs;

        HazardCell() {
            refs = new atomic<E*>[numOfRefs]{nullptr};
            toDeleteRefs = new atomic<E*>[numOfToDeleteRefs]{nullptr};
        }

        ~HazardCell(){
            delete[] refs;
            delete[] toDeleteRefs;
        }
    };

private:
    int numOfCells;
    HazardCell<T>* cells;

public:
    explicit HazardDomain(int maxNumOfThreads = 64) {
        numOfCells = maxNumOfThreads;
        cells = new HazardCell<T>[numOfCells];
    }

    ~HazardDomain() {
        T* p;
        for (int i = 0; i < numOfCells; ++i) {
            for (int j = 0; j < HazardCell<T>::numOfToDeleteRefs; ++j) {
                p = cells[i].toDeleteRefs[j].load();
                if(p != nullptr) {
                    wipeToDeleteRef(p);
                    delete p; // TODO HERE
                }
            }
        }
        delete[] cells;
    }

    int acquireCell() {
        bool cellIsFree;
        int i = 0;
        while(true) {
            cellIsFree = cells[i].isFree.load();
            if(cellIsFree && cells[i].isFree.compare_exchange_strong(cellIsFree, false)) {
                break;
            }
            i = (i+1)%numOfCells;
        }
        return i;
    }

    void putPtr(T* ptr, int cellIndex, int refIndex) {
        cells[cellIndex].refs[refIndex].store(ptr);
    }

    void deletePtr(T* ptr, int hzExceptCellIndex) {
        if(!putToDeleteRefs(ptr, hzExceptCellIndex)) {
            T* p;
            for (int i = 0; i < HazardCell<T>::numOfToDeleteRefs; ++i) {
                p = cells[hzExceptCellIndex].toDeleteRefs[i].load();
                if(!containsPtrExcept(p, hzExceptCellIndex)) {
                    cells[hzExceptCellIndex].toDeleteRefs[i].store(nullptr);
                    delete p;
                }
            }
            putToDeleteRefs(ptr, hzExceptCellIndex);
        }
    }

    void releaseCell(int cellIndex) {
        for (int i = 0; i < HazardCell<T>::numOfRefs; ++i) {
            cells[cellIndex].refs[i].store(nullptr);
        }
        cells[cellIndex].isFree.store(true);
    }

private:
    bool containsPtrExcept(T* ptr, int hzExceptCellIndex) {
        for (int i = 0; i < numOfCells; ++i) {
            if(i != hzExceptCellIndex && !cells[i].isFree.load()) {
                for (int j = 0; j < HazardCell<T>::numOfRefs; ++j) {
                    if(cells[i].refs[j].load() == ptr) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool containsToDeleteRef(T* ptr, int hzExceptCellIndex) {
        for (int i = 0; i < numOfCells; ++i) {
                for (int j = 0; j < HazardCell<T>::numOfRefs; ++j) {
                    if(cells[i].refs[j].load() == ptr) {
                        return true;
                    }
                }
        }
        return false;
    }

    bool putToDeleteRefs(T *ptr, int cellIndex) {
        for (int i = 0; i < HazardCell<T>::numOfToDeleteRefs; ++i) {
            if(cells[cellIndex].toDeleteRefs[i].load() == nullptr) {
                cells[cellIndex].toDeleteRefs[i].store(ptr);
                return true;
            }
        }
        return false;
    }

    void wipeToDeleteRef(T* ptr) {
        T* p;
        for (int i = 0; i < numOfCells; ++i) {
            for (int j = 0; j < HazardCell<T>::numOfToDeleteRefs; ++j) {
                p = cells[i].toDeleteRefs[j].load();
                if(p == ptr) {
                    cells[i].toDeleteRefs[j].store(nullptr);
                }
            }
        }
    }
};
