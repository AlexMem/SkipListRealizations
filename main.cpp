#define _WIN32_WINNT 0x0501
#include <time.h>
#include <thread>
#include <mingw.thread.h>
#include <unistd.h>
#include "concurrent_lockfree_skiplist.h"

using namespace std;

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



//14 adds, 1 remove
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
//    if(list->contains(-12)) {
//        cout << "contains -12" << endl;
        list->remove(-12);
//    }
    list->add(55);
    list->add(30);
}

//12 adds, 1 remove
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
}

//11 adds, 1 remove
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
}

void randRoutine(ConcurrentSkipList<int>* list, int seed) {
    srand(seed);
    int added = 0;
    int removed = 0;
    for (int i = 0; i < 255; ++i) {
//        if(rand()%4) {
            list->add(rand()%20000)?added++:added;
//        } else {
//            list->remove(rand()%10000)?removed++:removed;
//        }
    }
//    cout << added << ' ' << removed << endl;
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

void repeatTest(int times) {
    for (int i = 0; i < times; ++i) {
        ConcurrentSkipList<int> list;
        randTest(&list);
//        list.out();
    }
}


int main() {
//    ConcurrentSkipList<int> list;
//    fixedTest(&list);
//    list.out();

    shared_ptr<int> p1(new int(5));

    repeatTest(2555);

    /*int *a = new int;
    int *b = new int;
    *a = 5;
    *b = 7;
    MarkableReference<int> ma(a, false);
    cout << &ma << endl;
    atomic<MarkableReference<int>> ama(ma);
    cout << ama.is_lock_free() << " " << *ama.load().getRef() << endl;
    MarkableReference<int> _ma = ama.load();
    cout << &_ma << endl;
    MarkableReference<int> mb(b, false);
    cout << ama.compare_exchange_strong(_ma, mb) << endl;
    cout << ama.is_lock_free() << " " << *ama.load().getRef() << endl;*/

    return 0;
}
