#include <iostream>
#include <time.h>
#include <atomic>
#include <bitset>

using namespace std;

template<class V> class MarkableReference {
private:
    static const uintptr_t mask = 0b10000000000000000000000000000000;
    uintptr_t val;
public:
    MarkableReference(V* ref = nullptr, bool mark = false) {
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
        return false;
    }

    T find(T value, MarkableReference<Node<T>>** preds, MarkableReference<Node<T>>** succs) {
        int botLvl = 0;
        MarkableReference<Node<T>>* pred;
        MarkableReference<Node<T>>* curr;
        MarkableReference<Node<T>>* succ;

    retry:
        while (true) {
            pred = new MarkableReference(head, false);
            for (int lvl = MAXHEIGHT-1; lvl >= botLvl; --lvl) {
                curr = pred->getRef()->nexts[lvl].load();
                while (true) {
                    succ = curr->getRef()->nexts[lvl].load();
                    while (succ->getMark()){
                        if(!pred->getRef()->nexts[lvl].compare_exchange_strong(curr, succ)) {
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
            if(!pred->getRef()->nexts[botLvl].compare_exchange_strong(succ, new MarkableReference(newNode, false))) {
                continue;
            }

            for (int lvl = botLvl+1; lvl <= topLvl; ++lvl) {
                while(true) {
                    pred = preds[lvl];
                    succ = succs[lvl];
                    if(pred->getRef()->nexts[lvl].compare_exchange_strong(succ, new MarkableReference(newNode, false))) {
                        break;
                    }
                    find(value, preds, succs);
                }
            }

            return true;
        }
    }

    void remove(T value) {

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
                    std::cout << p->nexts[i].load()->getRef()->value << "\t";
                } else {
                    std::cout << "n\t";
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

int main() {
    ConcurrentSkipList<int> list;
    notRandInit(&list);
    list.out();
//
//    std::cout << std::endl;
//
//    list.remove(-2);
//    list.remove(15);
//    list.remove(8);
//    list.out();

    return 0;
}
