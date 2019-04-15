#define _WIN32_WINNT 0x0501
#include <iostream>
#include <time.h>
#include <atomic>
#include <thread>
#include <mingw.thread.h>


using namespace std;

template<class V> class MarkableReference {
private:
    static const uintptr_t mask = 0b10000000000000000000000000000000;
    uintptr_t val;
public:
    MarkableReference(V* ref, bool mark) {
        val = ((uintptr_t)ref & ~mask) | (mark ? mask : 0);
    }

    V* getRef() {
        return (V*)(val & ~mask);
    }

    bool getMark() {
        return (val & mask);
    }
};

template <class T> class ConcurrentSkipList {
    template <class E> class Node {
    public:
        E value;
        int level;
        atomic<MarkableReference<Node<E>>*>* nexts;

        Node() {
            nexts = new atomic<MarkableReference<Node<E>>*>[MAXHEIGHT];
            level = 0;
        }

        Node(E value, int lvl) {
            this->value = value;
            nexts = new atomic<MarkableReference<Node<E>>*>[lvl+1];
            level = lvl;
        }

        ~Node() {
            delete[] nexts;
        }
    };

// FIELDS
private:
    const double P = 0.5;
    const static int MAXHEIGHT = 20;

    Node<T>* head;
    int height;
    Node<T>* nil;

// CONSTRUCTORS
public:
    ConcurrentSkipList() {
        srand(time(0));
        nil = new Node<T>();

        head = new Node<T>();
        for(int i = 0; i < MAXHEIGHT; ++i) {
            head->nexts[i] = new MarkableReference<Node<T>>(nil, false);
        }
        head->level = MAXHEIGHT-1;

        height = 1;
    }

// PUBLIC METHODS
public:
    bool contains(T value) {
        int botLvl = 0;
        MarkableReference<Node<T>>* pred = new MarkableReference<Node<T>>(head, false);
        MarkableReference<Node<T>>* curr;
        MarkableReference<Node<T>>* succ;

        for (int lvl = MAXHEIGHT-1; lvl >= botLvl; --lvl) {
            curr = pred->getRef()->nexts[lvl].load();
            while(true) {
                succ = curr->getRef()->nexts[lvl].load();
                while(curr->getRef() != nil && succ->getMark()) {
                    curr = pred->getRef()->nexts[lvl].load();
                    succ = curr->getRef()->nexts[lvl].load();
                }
                if(compare(curr->getRef(), value) < 0) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        return compare(curr->getRef(), value) == 0;
    }

    bool find(T value, MarkableReference<Node<T>>** preds, MarkableReference<Node<T>>** succs) {
        int botLvl = 0;
        MarkableReference<Node<T>>* pred;
        MarkableReference<Node<T>>* curr;
        MarkableReference<Node<T>>* succ;

    retry:
        while (true) {
            pred = new MarkableReference<Node<T>>(head, false);
            for (int lvl = MAXHEIGHT-1; lvl >= botLvl; --lvl) {
                curr = pred->getRef()->nexts[lvl].load();
                while (true) {
                    succ = curr->getRef()->nexts[lvl].load();
                    while (curr->getRef() != nil && succ->getMark()){
                        if(!pred->getRef()->nexts[lvl].compare_exchange_strong(curr, new MarkableReference<Node<T>>(succ->getRef(), false))) {
                            goto retry;
                        }
                        curr = pred->getRef()->nexts[lvl].load();
                        succ = curr->getRef()->nexts[lvl].load();
                    }

                    if(compare(curr->getRef(), value) < 0) {
                        pred = curr;
                        curr = succ;
                    } else {
                        break;
                    }
                }

                preds[lvl] = pred;
                succs[lvl] = curr;
            }

            return compare(curr->getRef(), value) == 0;
        }
    }

    bool add(T value) {
        int topLvl = randomLevel();
        int botLvl = 0;
        MarkableReference<Node<T>>** preds = new MarkableReference<Node<T>>*[MAXHEIGHT];
        MarkableReference<Node<T>>** succs = new MarkableReference<Node<T>>*[MAXHEIGHT];

        while(true) {
            if(find(value, preds, succs)){
                return false;
            }

            Node<T>* newNode = new Node<T>(value, topLvl);
            for (int lvl = botLvl; lvl <= topLvl; ++lvl) {
                newNode->nexts[lvl].store(succs[lvl]);
            }
            MarkableReference<Node<T>>* pred = preds[botLvl];
            MarkableReference<Node<T>>* succ = succs[botLvl];
            newNode->nexts[botLvl].store(succ);
            if(!pred->getRef()->nexts[botLvl].compare_exchange_strong(succ, new MarkableReference<Node<T>>(newNode, false))) {
                continue;
            }

            for (int lvl = botLvl+1; lvl <= topLvl; ++lvl) {
                while(true) {
                    pred = preds[lvl];
                    succ = succs[lvl];
                    if(pred->getRef()->nexts[lvl].compare_exchange_strong(succ, new MarkableReference<Node<T>>(newNode, false))) {
                        break;
                    }
                    find(value, preds, succs);
                }
            }

            return true;
        }
    }

    bool remove(T value) {
        int botLvl = 0;
        MarkableReference<Node<T>>** preds = new MarkableReference<Node<T>>*[MAXHEIGHT];
        MarkableReference<Node<T>>** succs = new MarkableReference<Node<T>>*[MAXHEIGHT];
        MarkableReference<Node<T>>* succ;

        while(true) {
            if(!find(value, preds, succs)) {
                return false;
            }

            MarkableReference<Node<T>>* toRemove = succs[botLvl];
            for (int lvl = toRemove->getRef()->level; lvl >= botLvl; --lvl) {
                succ = toRemove->getRef()->nexts[lvl].load();
                while(!succ->getMark()) {
                    toRemove->getRef()->nexts[lvl].compare_exchange_strong(succ, new MarkableReference<Node<T>>(succ->getRef(), true));
                    succ = toRemove->getRef()->nexts[lvl].load();
                }
            }

            succ = toRemove->getRef()->nexts[botLvl].load();
            while(true) {
                bool markedIt = toRemove->getRef()->nexts[botLvl].compare_exchange_strong(succ, new MarkableReference<Node<T>>(succ->getRef(), true));
                succ = succs[botLvl]->getRef()->nexts[botLvl].load();
                if(markedIt) {
                    find(value, preds, succs);
                    return true;
                } else {
                    if(succ->getMark()) {
                        return false;
                    }
                }
            }
        }
    }

    void out() {
        for (Node<T> *p = head; p!=nil; p=p->nexts[0].load()->getRef()) {
            if(p == head) {
                std::cout << "h [" << p->level << "]\t->\t";
            } else {
                std::cout << p->value << " [" << p->level << "]\t->\t";
            }
            for (int i = 0; i <= p->level; ++i) {
                if(p->nexts[i].load()->getRef() != nil) {
                    std::cout << p->nexts[i].load()->getRef()->value << "[" << p->nexts[i].load()->getMark() << "]\t";
                } else {
                    std::cout << "n[0]\t";
                }
            }
            std::cout << std::endl;
        }
    }

// PRIVATE METHODS
private:
    int randomLevel() {
        int lvl = 0;
        while ((double)rand()/(double)RAND_MAX < P && lvl < MAXHEIGHT) {
            ++lvl;
        }

        return lvl;
    }

    int compare(Node<T>* node, T other) {
        if (node == nil) return 1;
        if (node->value < other) return -1;
        if (node->value == other) return 0;
        return 1;
    }
};

void randInit(ConcurrentSkipList<int> *list) {
    for(int i = 0; i < 10; ++i) {
        list->add((rand() % 2 ? -1 : 1) * rand());
    }
}

void notRandInit(ConcurrentSkipList<int> *list) {
    list->add(-2);
    list->add(7);
    list->add(0);
    list->add(1);
    list->add(8);
    list->add(15);
    list->add(10);
}



void routine1(ConcurrentSkipList<int>* list) {
    list->add(-2);
    list->add(7);
    list->add(0);
    list->add(1);
    list->add(8);
    list->add(15);
    list->add(10);
    list->add(-22);
    list->add(-77);
    list->add(-4);
    list->add(-1);
    list->add(87);
    if(list->contains(-12)) {
        cout << "contains -12" << endl;
        list->remove(-12);
    }
    list->add(55);
    list->add(30);
    //13 adds, 1 remove
}

void routine2(ConcurrentSkipList<int>* list) {
    list->add(-12);
    list->add(57);
    list->add(5);
    list->add(4);
    list->add(12);
    list->add(9);
    list->add(2);
    list->remove(-2);
    list->add(37);
    list->add(-44);
    list->add(-27);
    list->add(14);
    list->add(26);
    //12 adds, 1 remove
}

void routine3(ConcurrentSkipList<int>* list) {
    list->add(-13);
    list->add(59);
    list->add(68);
    list->add(89);
    list->add(-9);
    list->add(63);
    list->add(17);
    list->remove(-2);
    list->add(-99);
    list->add(-47);
    list->add(34);
    list->add(27);
    //11 adds, 1 remove
}

void randRoutine(ConcurrentSkipList<int>* list, int seed) {
    srand(seed);
    int k = 0;
    for (int i = 0; i < 25; ++i) {
        list->add(rand()%10000)?k++:k;
    }
    cout << k << endl;
}

void fixedTest(ConcurrentSkipList<int>* list) {
    std::thread t1(routine1, list);
    std::thread t2(routine2, list);
    std::thread t3(routine3, list);

    t1.join();
    t2.join();
    t3.join();
}

void randTest(ConcurrentSkipList<int>* list) {
    std::thread t1(randRoutine, list, time(nullptr)*2);
    std::thread t2(randRoutine, list, time(nullptr)*3);
    std::thread t3(randRoutine, list, time(nullptr)*5);
    std::thread t4(randRoutine, list, time(nullptr)*7);
    std::thread t5(randRoutine, list, time(nullptr)*11);
    std::thread t6(randRoutine, list, time(nullptr)*13);
    std::thread t7(randRoutine, list, time(nullptr)*17);
    std::thread t8(randRoutine, list, time(nullptr)*19);

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();
    t7.join();
    t8.join();
}

int main() {
    ConcurrentSkipList<int> list;
//    notRandInit(&list);
//    list.out();

//    std::cout << std::endl;

//    list.remove(-2);
//    list.remove(15);
//    list.remove(8);

    fixedTest(&list);

    list.out();

    return 0;
}
