#include <iostream>
#include <atomic>
#include <random>

using namespace std;

template<class V> class MarkableReference {
private:
    static const uintptr_t mask = 0b10000000000000000000000000000000;
    uintptr_t val;
public:
    MarkableReference() {
        val = 0;
    }

    MarkableReference(V* ref, bool mark) {
        val = ((uintptr_t)ref & ~mask) | (mark ? mask : 0);
    }

    V* getRef() {
        return (V*)(val & ~mask);
    }

    bool getMark() {
        return (val & mask);
    }

    void setRef(V* ref) {
        val = ((uintptr_t)ref & ~mask) | (val & mask);
    }
};

template <class T> class ConcurrentSkipList {
    template <class E> class Node {
    public:
        E value;
        int level;
        atomic<MarkableReference<Node<E>>>** nexts;

        Node() {
            nexts = new atomic<MarkableReference<Node<E>>>*[MAXHEIGHT];
            MarkableReference<Node<E>> mock;
            for (int i = 0; i < MAXHEIGHT; ++i) {
                nexts[i] = new atomic<MarkableReference<Node<E>>>(mock);
            }
            level = MAXHEIGHT-1;
        }

        Node(E value, int lvl) {
            this->value = value;
            nexts = new atomic<MarkableReference<Node<E>>>*[lvl+1];
            MarkableReference<Node<E>> mock;
            for (int i = 0; i <= lvl; ++i) {
                nexts[i] = new atomic<MarkableReference<Node<E>>>(mock);
            }
            level = lvl;
        }

        ~Node() {
            for (int i = 0; i <= level; ++i) {
                delete nexts[i];
            }
            delete[] nexts;
        }
    };

// FIELDS
private:
    const double P = 0.5;
    const static int MAXHEIGHT = 20;

    random_device randomDevice;

    Node<T>* head;
    Node<T>* tail;
    int height;

// CONSTRUCTORS
public:
    ConcurrentSkipList() {
        tail = new Node<T>();

        head = new Node<T>();
        MarkableReference<Node<T>> markableTail(tail, false);
        for(int i = 0; i < MAXHEIGHT; ++i) {
            head->nexts[i]->store(markableTail);
        }

        height = 1;
    }

    ~ConcurrentSkipList() {
        Node<T> *toDel;
        for (Node<T> *p = head; p!=tail;) {
            toDel = p;
            p=p->nexts[0]->load().getRef();
            delete toDel;
        }
        delete tail;
    }

// PUBLIC METHODS
public:
    bool contains(T value) {
        int botLvl = 0;

        MarkableReference<Node<T>> pred(head, false);
        MarkableReference<Node<T>> curr;
        MarkableReference<Node<T>> succ;

        for (int lvl = MAXHEIGHT-1; lvl >= botLvl; --lvl) {
            curr = pred.getRef()->nexts[lvl]->load();
            while(true) {
                succ = curr.getRef()->nexts[lvl]->load();
                while(curr.getRef() != tail && succ.getMark()) { // optimization?
                    curr = pred.getRef()->nexts[lvl]->load();
                    succ = curr.getRef()->nexts[lvl]->load();
                }
                if(compare(curr.getRef(), value) < 0) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }

        return compare(curr.getRef(), value) == 0;
    }

    bool find(T value, MarkableReference<Node<T>>* preds, MarkableReference<Node<T>>* succs) {
        int botLvl = 0;
        MarkableReference<Node<T>> markableHead(head, false);

        MarkableReference<Node<T>> pred;
        MarkableReference<Node<T>> curr;
        MarkableReference<Node<T>> succ;

        retry:
        while (true) {
            pred = markableHead;
            for (int lvl = MAXHEIGHT-1; lvl >= botLvl; --lvl) {
                curr = pred.getRef()->nexts[lvl]->load();
                while (true) {
                    succ = curr.getRef()->nexts[lvl]->load();
                    while (curr.getRef() != tail && succ.getMark()){
                        MarkableReference<Node<T>> newSucc(succ.getRef(), false);
                        if(!pred.getRef()->nexts[lvl]->compare_exchange_strong(curr, newSucc)) {
                            goto retry;
                        }
                        // linearization point if lvl == 0
                        curr = pred.getRef()->nexts[lvl]->load();
                        succ = curr.getRef()->nexts[lvl]->load();
                    }

                    if(compare(curr.getRef(), value) < 0) {
                        pred = curr;
                        curr = succ;
                    } else {
                        break;
                    }
                }

                preds[lvl] = pred;
                succs[lvl] = curr;
            }

            return compare(curr.getRef(), value) == 0;
        }
    }

    bool add(T value) {
        int topLvl = randomLevel();
        int botLvl = 0;
        MarkableReference<Node<T>>* preds = new MarkableReference<Node<T>>[MAXHEIGHT];
        MarkableReference<Node<T>>* succs = new MarkableReference<Node<T>>[MAXHEIGHT];

        while(true) {
            if(find(value, preds, succs)){
                delete[] preds;
                delete[] succs;
                return false;
            }

            Node<T>* newNode = new Node<T>(value, topLvl);
            for (int lvl = botLvl; lvl <= topLvl; ++lvl) {
                newNode->nexts[lvl]->store(succs[lvl]);
            }
            MarkableReference<Node<T>> pred = preds[botLvl];
            MarkableReference<Node<T>> succ = succs[botLvl];
            newNode->nexts[botLvl]->store(succ);
            MarkableReference<Node<T>> markableNewNode(newNode, false);
            if(!pred.getRef()->nexts[botLvl]->compare_exchange_strong(succ, markableNewNode)) {
                delete newNode;
                continue;
            }
            // linearization point

            for (int lvl = botLvl+1; lvl < topLvl; ++lvl) {
                while(true) {
                    pred = preds[lvl];
                    succ = succs[lvl];
                    if(pred.getRef()->nexts[lvl]->compare_exchange_strong(succ, markableNewNode)) {
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
        int botLvl = 0;

        MarkableReference<Node<T>>* preds = new MarkableReference<Node<T>>[MAXHEIGHT];
        MarkableReference<Node<T>>* succs = new MarkableReference<Node<T>>[MAXHEIGHT];
        MarkableReference<Node<T>> succ;

        while(true) {
            if(!find(value, preds, succs)) {
                delete[] preds;
                delete[] succs;
                return false;
            }

            Node<T>* toRemove = succs[botLvl].getRef();
            for (int lvl = toRemove->level; lvl >= botLvl+1; --lvl) {
                succ = toRemove->nexts[lvl]->load();
                while(!succ.getMark()) {
                    MarkableReference<Node<T>> newSucc(succ.getRef(), true);
                    toRemove->nexts[lvl]->compare_exchange_strong(succ, newSucc);
                    succ = toRemove->nexts[lvl]->load();
                }
            }

            succ = toRemove->nexts[botLvl]->load();
            while(true) {
                MarkableReference<Node<T>> newSucc(succ.getRef(), true);
                bool markedIt = toRemove->nexts[botLvl]->compare_exchange_strong(succ, newSucc);
                // linearization point if markedIf == true
                succ = succs[botLvl].getRef()->nexts[botLvl]->load();
                if(markedIt) {
                    find(value, preds, succs);
//                    cout << endl << "Deleting " << dec << toRemove->nexts[botLvl]->is_lock_free() << " " << toRemove->value << " " << hex << toRemove << endl;
//                    delete toRemove; // TODO fix it
//                    cout << "deleted" << endl;

                    delete[] preds;
                    delete[] succs;
                    return true;
                } else {
                    if(succ.getMark()) {
                        delete[] preds;
                        delete[] succs;
                        return false;
                    }
                }
            }
        }
    }

    void out() {
        for (Node<T> *p = head; p!=tail; p=p->nexts[0]->load().getRef()) {
            if(p == head) {
                std::cout << "h [" << p->level << "]\t->\t";
            } else {
                std::cout << p->value << " [" << p->level << "]\t->\t";
            }
            for (int i = 0; i <= p->level; ++i) {
                if(p->nexts[i]->load().getRef() != tail) {
                    std::cout << p->nexts[i]->load().getRef()->value << "[" << p->nexts[i]->load().getMark() << "]\t";
                } else {
                    std::cout << "n[0]\t";
                }
            }
            std::cout << std::endl;
        }
    }

    void checkLockFree() {
        for(Node<T>* curr = head; curr != tail; curr = curr->nexts[0]->load().getRef()) {
            cout << "Node " << curr->value << ":" << endl;
            checkNodeForLockFree(curr);
        }
    }

// PRIVATE METHODS
private:
    int randomLevel() {
        int lvl = 0;
        std::mt19937 mt(randomDevice());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        while (dist(mt) < P && lvl < MAXHEIGHT-1) {
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
        cout << "\tValue: " << node->value << endl;
        for(int i = 0; i <= node->level; ++i) {
            cout << "\t" << i << ". is LF: " << node->nexts[i].is_lock_free() << endl;
        }
    }
};