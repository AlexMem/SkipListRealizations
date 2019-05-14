#include <atomic>

template<class T> class HazardDomain {
    template<class E> class HazardCell {
    public:
        // 0 - pred
        // 1 - curr
        // 2 - succ
        // 3 - newNode/toRemove
        // 4-(numOfRefs-1) - preds and succs

        int currentToDeleteIndex;

        std::atomic<bool> isFree{true};
        std::atomic<E*>* safeRefs;
        std::atomic<E*>* deleteRefs;

        explicit HazardCell(int numOfSafeRefs, int numOfDeleteRefs) {
            currentToDeleteIndex = 0;
            safeRefs = new std::atomic<E*>[numOfSafeRefs]{nullptr};
            deleteRefs = new std::atomic<E*>[numOfDeleteRefs]{nullptr};
        }

        ~HazardCell(){
            delete[] safeRefs;
            delete[] deleteRefs;
        }
    };

private:
    unsigned int numOfCells;

    unsigned int numOfSafeRefsPerCell;
    unsigned int numOfDeleteRefsPerCell;

    HazardCell<T>** cells;

public:
    explicit HazardDomain(unsigned int numOfSafeRefs = 45, unsigned int maxNumOfThreads = 8) {
        numOfCells = maxNumOfThreads;
        numOfSafeRefsPerCell = numOfSafeRefs;
        numOfDeleteRefsPerCell = (unsigned int)1.5*numOfSafeRefs*maxNumOfThreads;

        cells = new HazardCell<T>*[numOfCells];
        for (int i = 0; i < numOfCells; ++i) {
            cells[i] = new HazardCell<T>(numOfSafeRefsPerCell, numOfDeleteRefsPerCell);
        }
    }

    ~HazardDomain() {
        T* p;
        for (int i = numOfCells-1; i >= 0; i = --numOfCells-1) {
            for (int j = 0; j < numOfDeleteRefsPerCell; ++j) {
                p = cells[i]->deleteRefs[j].load(std::memory_order_relaxed);
                if(p != nullptr) {
                    wipeDeleteRef(p);
                    delete p;
                }
            }
            delete cells[i];
        }
        delete[] cells;
    }

    int acquireCell() {
        bool cellIsFree;
        int i = 0;
        while(true) {
            cellIsFree = cells[i]->isFree.load(std::memory_order_acquire);
            if(cellIsFree && cells[i]->isFree.compare_exchange_strong(cellIsFree, false, std::memory_order_release)) {
                break;
            }
            i = (i+1)%numOfCells;
        }
        return i;
    }

    void releaseCell(int cellIndex) {
        for (int i = 0; i < numOfSafeRefsPerCell; ++i) {
            cells[cellIndex]->safeRefs[i].store(nullptr, std::memory_order_relaxed);
        }
        cells[cellIndex]->isFree.store(true, std::memory_order_release);
    }

    T* protect(T* ptr, int cellIndex, int refIndex) {
        cells[cellIndex]->safeRefs[refIndex].store(ptr, std::memory_order_release);
        return ptr;
    }

    T* protectWithValidation(AtomicMarkableReference<T> &sourceRef, int cellIndex, int refIndex) {
        T* ptr;
        do {
            ptr = sourceRef.getRef();
            cells[cellIndex]->safeRefs[refIndex].store(ptr);
        } while(sourceRef.getRef() != ptr);
        return ptr;
    }

    T* protectWithValidation(AtomicMarkableReference<T> &sourceRef, bool &mark, int cellIndex, int refIndex) {
        T* ptr;
        do {
            ptr = sourceRef.getRefAndMark(mark);
            cells[cellIndex]->safeRefs[refIndex].store(ptr);
        } while(sourceRef.getRef() != ptr);
        return ptr;
    }

    void deletePtr(T* ptr, int hzExceptCellIndex) {
        int currIndex = cells[hzExceptCellIndex]->currentToDeleteIndex;
        T* p = cells[hzExceptCellIndex]->deleteRefs[currIndex].load(std::memory_order_acquire);
        if(p == nullptr) {
            cells[hzExceptCellIndex]->deleteRefs[currIndex].store(ptr, std::memory_order_release);
        } else {
            while(true) {
                if (!containsPtrExcept(p, hzExceptCellIndex)) {
                    cells[hzExceptCellIndex]->deleteRefs[currIndex].store(ptr);
                    delete p;
                    break;
                }
                currIndex = (currIndex+1)%numOfDeleteRefsPerCell;
                p = cells[hzExceptCellIndex]->deleteRefs[currIndex].load(std::memory_order_relaxed); // TODO mb danger
            }
        }
        currIndex = (currIndex+1)%numOfDeleteRefsPerCell;
        cells[hzExceptCellIndex]->currentToDeleteIndex = currIndex;
    }

private:
    bool containsPtrExcept(T* ptr, int hzExceptCellIndex) {
        for (int i = 0; i < numOfCells; ++i) {
            if(i != hzExceptCellIndex && !cells[i]->isFree.load(std::memory_order_relaxed)) {
                for (int j = 0; j < numOfSafeRefsPerCell; ++j) {
                    if(cells[i]->safeRefs[j].load(std::memory_order_relaxed) == ptr) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void wipeDeleteRef(T *ptr) {
        T* p;
        for (int i = 0; i < numOfCells; ++i) {
            for (int j = 0; j < numOfDeleteRefsPerCell; ++j) {
                p = cells[i]->deleteRefs[j].load(std::memory_order_acquire);
                if(p == ptr) {
                    cells[i]->deleteRefs[j].store(nullptr, std::memory_order_release);
                }
            }
        }
    }

//UNUSED
    /*void deletePtr(T* ptr, int hzExceptCellIndex) {
        if(!putToDeleteRefs(ptr, hzExceptCellIndex)) {
            T* p;
            for (int i = 0; i < numOfDeleteRefsPerCell; ++i) {
                p = cells[hzExceptCellIndex]->deleteRefs[i].load();
                if(!containsPtrExcept(p, hzExceptCellIndex)) {
                    cells[hzExceptCellIndex]->deleteRefs[i].store(nullptr);
                    delete p;
                }
            }
            putToDeleteRefs(ptr, hzExceptCellIndex);
        }
    }*/

    bool putToDeleteRefs(T *ptr, int cellIndex) {
        for (int i = 0; i < numOfDeleteRefsPerCell; ++i) {
            if(cells[cellIndex]->deleteRefs[i].load() == nullptr) {
                cells[cellIndex]->deleteRefs[i].store(ptr);
                return true;
            }
        }
        return false;
    }
};
