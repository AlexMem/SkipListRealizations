#include <iostream>
#include <time.h>

template <class T> class ConcurrentSkipList {
    template <class E> class Node {
    public:
        E value;
        int level;
        Node<E>** nexts;

        Node() {
            nexts = nullptr;
            level = 0;
        }

        Node(E value, int lvl) {
            this->value = value;
            nexts = new Node<E>*[lvl+1];
            level = lvl;
        }

        ~Node() {
            delete[] nexts;
        }
    };

// FIELDS
private:
    const double P = 0.5;
    const int MAXHEIGHT = 20;

    Node<T>* head;
    int height;
    Node<T>* nil;

// CONSTRUCTORS
public:
    ConcurrentSkipList() {
        srand(time(0));
        nil = new Node<T>();

        head = new Node<T>();
        head->nexts = new Node<T>*[MAXHEIGHT];
        for(int i = 0; i < MAXHEIGHT; ++i) {
            head->nexts[i] = nil;
        }
        head->level = MAXHEIGHT-1;

        height = 1;
    }

// PUBLIC METHODS
public:
    T search(T value) {
        Node<T>* p = head;
        for (int i = height - 1; i >= 0; --i) {
            while (p->nexts[i] != nil && compare(p->nexts[i], value) < 0) {
                p = p->nexts[i];
            }
        }

        p = p->nexts[0];
        if (compare(p, value) == 0) {
            return p->value;
        }
        throw std::runtime_error("Value not found");
    }

    void insert(T value) {
        Node<T>** update = new Node<T>*[MAXHEIGHT];
        Node<T>* p = head;
        for (int i = height - 1; i >= 0; --i) {
            while (p->nexts[i] != nil && compare(p->nexts[i], value) < 0) {
                p = p->nexts[i];
            }

            update[i] = p;
        }

        p = p->nexts[0];
        if (compare(p, value) != 0) {
            int lvl = randomLevel();
            if (lvl >= height) {
                for (int i = height; i <= lvl; ++i) {
                    update[i] = head;
                    height = lvl+1;
                }
            }

            p = new Node<T>(value, lvl);
            for (int i = 0; i <= lvl; ++i) {
                p->nexts[i] = update[i]->nexts[i];
                update[i]->nexts[i] = p;
            }
        } else {
            p->value = value;
        }

        delete[] update;
    }

    void deleteElement(T value) {
        Node<T>** update = new Node<T>*[MAXHEIGHT];
        Node<T>* p = head;
        for (int i = height - 1; i >= 0; --i) {
            while (p->nexts[i] != nil && compare(p->nexts[i], value) < 0) {
                p = p->nexts[i];
            }

            update[i] = p;
        }

        p = p->nexts[0];
        if (compare(p, value) == 0) {
            for (int i = 0; i < height; ++i) {
                if (update[i]->nexts[i] != p) break;
                update[i]->nexts[i] = p->nexts[i];
            }
            delete p;

            while (height > 1 && head->nexts[height-1] == nil) {
                --height;
            }
        }

        delete[] update;
    }

    void out() {
        for (auto *p = head; p!=nil; p=p->nexts[0]) {
            if(p == head) {
                std::cout << "h [" << p->level << "]\t->\t";
            } else {
                std::cout << p->value << " [" << p->level << "]\t->\t";
            }
            for (int i = 0; i <= p->level; ++i) {
                if(p->nexts[i] != nil) {
                    std::cout << p->nexts[i]->value << "\t";
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
        list->insert((rand()%2? -1:1)*rand());
    }
}

void notRandInit(ConcurrentSkipList<int> *list) {
    list->insert(-2);
    list->insert(7);
    list->insert(0);
    list->insert(1);
    list->insert(8);
    list->insert(15);
    list->insert(10);
}

int main() {
    ConcurrentSkipList<int> list;
    notRandInit(&list);
    list.out();

    std::cout << std::endl;

    list.deleteElement(-2);
    list.deleteElement(15);
    list.deleteElement(8);
    list.out();
    return 0;
}
