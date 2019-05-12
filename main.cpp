#define _WIN32_WINNT 0x0501
#include <ctime>
#include <future>
#include <thread>
#include <mingw.thread.h>
#include <fstream>
#include <unistd.h>
#include "concurrent_lockfree_skiplist.h"

using namespace std;

std::random_device rd;

void randInit(ConcurrentSkipList<int> *list) {
    for(int i = 0; i < 3000; ++i) {
        list->add((rd() % 2 ? -1 : 1) * (rd()%30000));
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
    if(list->contains(-12)) {
        cout << "contains -12" << endl;
        list->remove(-12);
    }
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
//    if(list->remove(-2)) cout << "removed -2" << endl;
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
    list->remove(-12);
    list->add(63);
    list->add(17);
//    if(list->remove(-2)) cout << "removed -2" << endl;
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
    for (int i = 0; i < 3000; ++i) {
        if(rand()%2) {
            list->add(rand()%20000)?added++:added;
        } else {
            list->remove(rand()%20000)?removed++:removed;
        }
    }
    cout << added << ' ' << removed << endl;
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
        fixedTest(&list);
//        if(i%10 == 0) {
//            cout << i << endl;
//        }
//        if(list.remove(-12)) cout << "removed -12" << endl;
        list.print();
        cout << "Contains -12 = " << list.contains(-12) << endl;
    }
}



// [0]...contains...[b1]...add...[b2]...remove...[1]
void experimentRoutine(ConcurrentSkipList<int>* list, int numOfOperations, double b1, double b2, double& time) {
    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double decision;
    clock_t start;
    clock_t total = 0;
    for (int i = 0; i < numOfOperations; ++i) {
        decision = dist(mt);
        if(decision < b1) {
            start = clock();
            list->contains(rd()%30000);
            total += clock() - start;
        } else {
            if(decision < b2) {
                start = clock();
                list->add(rd()%30000);
                total += clock() - start;
            } else {
                start = clock();
                list->remove(rd()%30000);
                total += clock() - start;
            }
        }
    }
    time = ((double)total)/CLOCKS_PER_SEC;
//    cout << time << endl;
}

double experiment(double p, unsigned int maxHeight, unsigned int numOfThreads, int numOfOperations, double b1, double b2) {
    ConcurrentSkipList<int> list(maxHeight, numOfThreads, p);
    randInit(&list);
    double result = 0.0;
    double* times = new double[numOfThreads];
    std::thread** threads = new std::thread*[numOfThreads];
    for (int i = 0; i < numOfThreads; ++i) {
        threads[i] = new std::thread(experimentRoutine, &list, numOfOperations, b1, b2, times[i]);
    }
    for (int i = 0; i < numOfThreads; ++i) {
        threads[i]->join();
    }

    for (int i = 0; i < numOfThreads; ++i) {
//        cout << times[i] << endl;
        result += times[i];
    }
    result /= numOfOperations*numOfThreads;
//    list.print();
    return result;
}

void experiments(double b1, double b2, int numOfOperations, ofstream& file) {
    double p = 0.5;
    double result;

    for(int i = 0; i < 5; ++i) {
        file << "p = " << p << endl;
        for(unsigned int maxHeight = 5; maxHeight <= 40; maxHeight += 5) {
            cout << "p = " << p << ", maxHeight = " << maxHeight << endl;
            for (unsigned int k = 1; k <= 32; k *= 2) {
                result = experiment(p, maxHeight, k, numOfOperations, b1, b2);
                cout << '\t' << result << " s" <<  endl;
                file << result << '\t';
            }
            cout << endl;
            file << endl;
        }
        p = p + 0.1;
        file << endl;
    }
}



int main() {
//    bool mark = false;
//    AtomicMarkableReference<int> ref(new int(5), true);
//    int* ref5 = ref.getRefAndMark(mark);
//    cout << ref5 << " " << mark << endl;

//    repeatTest(1);
//    repeatTest(3577);

    int numOfOperations = 5000;
    ofstream file("results.txt");
    if(!file.is_open()) {
        cout << "Cannot create/open file result.txt" << endl;
        return -1;
    }

    cout.setf(ios::fixed);
    file.setf(ios::fixed);
    cout.precision(9);
    file.precision(9);

    cout << "Time in seconds" << endl << endl;
    file << "Time in seconds" << endl << endl;

    cout << "90%/5%/5%" << endl;
    file << "90%/5%/5%" << endl;
    experiments(0.9, 0.95, numOfOperations, file);  // 90% - contains
                                                    // 5% - add
                                                    // 5% - remove

    cout << endl <<  "80%/10%/10%" << endl;
    file << endl <<  "80%/10%/10%" << endl;
    experiments(0.8, 0.9, numOfOperations, file);   // 80% - contains
                                                    // 10% - add
                                                    // 10% - remove

    cout << endl <<  "33%/33%/33%" << endl;
    file << endl <<  "33%/33%/33%" << endl;
    experiments(0.34, 0.67, numOfOperations, file); // 33% - contains
                                                    // 33% - add
                                                    // 33% - remove

    file.close();
    return 0;
}
