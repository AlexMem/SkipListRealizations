#include <iostream>
#include <random>
#include "atomic_markable_reference.h"

//Класс потокобезопасного неблокирующего списка с пропусками
template <class T> class ConcurrentSkipList {
    //Класс узла списка с пропусками
    template <class E> class Node {
    public:
        //Элемент, хранимый в узле
        E value;
        //Уровень узла
        unsigned int level;
        //Массив маркируемых ссылок на следующие узлы
        AtomicMarkableReference<Node<E>>* nexts;
        //Указатель (в атомарном объекте) на следующий удаленный узел, используется только когда узел добавлен во Freelist
        std::atomic<Node<E>*> nextFree{nullptr};

        //Конструктор
        //lvl - уровень узла
        explicit Node(unsigned int lvl) {
            nexts = new AtomicMarkableReference<Node<E>>[lvl+1];
            level = lvl;
        }

        //Конструктор
        //value - элемент для хранения в списке
        //lvl - уровень узла
        Node(E value, unsigned int lvl) {
            this->value = value;
            nexts = new AtomicMarkableReference<Node<E>>[lvl+1];
            level = lvl;
        }

        ~Node() {
            delete[] nexts;
        }
    };

    //Класс списка удаленных узлов
    class FreeList {
        //Указатель на голову списка (в атомарном объекте)
        std::atomic<Node<T>*> freeListHead{nullptr};
        //Указатель на хвост списка (в атомарном объекте)
        std::atomic<Node<T>*> freeListTail{nullptr};
    public:
        FreeList() {
            Node<T>* headNode = new Node<T>(0);
            freeListHead.store(headNode);
            freeListTail.store(headNode);
        }

        //Добавляет узел в список удаленных узлов
        //newNode - указатель на удаленный узел
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
    //Вероятностный параметр, регулирует разброс генерируемых высот для новых узлов списка
    double P;
    //Максимальная высота списка
    unsigned int maxHeight;
    //Генератор случайных целых чисел
    std::random_device randomDevice;
    //Список удаленных узлов
    FreeList freeList;
    //Голова списка с пропусками
    Node<T>* head;
    //Хвост списка с пропусками
    Node<T>* tail;

// CONSTRUCTORS
public:
    //Конструктор
    //maxHeight - максимальная высота списка, по умолчанию = 10
    //P - параметр P списка, по умолчанию = 0.5
    explicit ConcurrentSkipList(unsigned int maxHeight = 10, double P = 0.5) {
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
    //Метод поиска
    //Осуществляет поиск элемента в списке с пропусками
    //value - искомый элемент
    //Возвращает    true - если элемент найден
    //              false - если элемент отсутствует
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

    //Внутренний метод поиска
    //Осуществляет поиск элемента в списке с перевязыванием помеченных узлов
    //value - искомый элемент
    //preds - массив указателей на предшествующие узлы (заполняется методом)
    //succs - массив указателей на последующие узлы (заполняется методом)
    //Возвращает    true - если элемент найден
    //              false - если элемент отсутствует
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
                while (true) {
                    succ = curr->nexts[lvl].getRefAndMark(mark);
                    while (curr != tail && mark){
                        if(!pred->nexts[lvl].CAS(curr, succ, false, false)) {
                            goto retry;
                        }
                        curr = pred->nexts[lvl].getRef();
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

    //Метод вставки
    //Вставляет переданный элемент в список с пропусками
    //value - вставляемый элемент
    //Возвращает    true - если элемент вставлен в список с пропусками
    //              false - если вставляемый элемент уже есть в списке с пропускми
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

    //Метод удаления
    //Удаляет переданный элемент из списка с пропусками
    //value - удаляемый элемент
    //Возвращает    true - если элемент удален тем потоком, который вызвал метод
    //              false - если удаляемого элемента нет в списке или он был удален другим потоком
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
    //Генерирует случайный уровень для нового узла
    unsigned int getRandomLevel() {
        unsigned int lvl = 0;
        std::mt19937 mt(randomDevice());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        while (dist(mt) < P && lvl < maxHeight-1) {
            ++lvl;
        }
        return lvl;
    }

    //Сравнивает элементы
    //node - указатель на узел с элементом для сравнения
    //other - другой элемент для сравнения
    //Возвращает   -1 - если node->value < other
    //              0 - если node->value = other
    //              1 - если node->value > other
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
