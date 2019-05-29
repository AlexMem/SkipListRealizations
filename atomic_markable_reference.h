#include <atomic>

//Класс атомарной маркируемой ссылки
//Позволяет атомарно работать со ссылкой и пометкой к ней
template<class V> class AtomicMarkableReference {
private:
    //Маска для выделения пометки
    static const uintptr_t mask = 1;
    //Значение, хранящее ссылку и пометку (в атомарном объекте)
    std::atomic<uintptr_t> val{0};
public:
    //Конструктор по умолчанию
    AtomicMarkableReference() = default;

    //Конструктор
    //ref - указатель на объект
    //mark - пометка
    AtomicMarkableReference(V* ref, bool mark) {
        val.store(convert(ref, mark));
    }

    //Возвращает указатель на объект
    V* getRef() {
        return (V*)(val.load(std::memory_order_acquire) & ~mask);
    }

    //Возвращает пометку
    bool getMark() {
        return static_cast<bool>(val.load(std::memory_order_acquire) & mask);
    }

    uintptr_t getVal() {
        return val.load(std::memory_order_acquire);
    }

    //Возвращает указатель на объект и пометку
    V* getRefAndMark(bool& mark) {
        uintptr_t _val = val.load(std::memory_order_acquire);
        mark = static_cast<bool>(_val & mask);
        return (V*)(_val & ~mask);
    }

    //Устанавливает новый указатель на объект и пометку
    void setVal(V* ref, bool mark) {
        val.store(convert(ref, mark));
    }

    bool CAS(uintptr_t& expected, V* newRef, bool newMark) {
        return val.compare_exchange_strong(expected, convert(newRef, newMark));
    }

    //CompareAndSwap операция
    //expRef - ожидаемая ссылка
    //newRef - новая ссылка
    //expMark - ожидаемая пометка
    //newMark - новая пометка
    //Возваращет    true - операция выполнена
    //              false - операция не выполнена
    bool CAS(V* expRef, V* newRef, bool expMark, bool newMark) {
        uintptr_t _val = convert(expRef, expMark);
        return val.compare_exchange_strong(_val, convert(newRef, newMark));
    }

    //Возвращает сслыку и пометку в одном значении
    static uintptr_t convert(V* ref, bool mark) {
        return ((uintptr_t)ref & ~mask) | (mark ? mask : 0);
    }

    bool is_lock_free() {
        return val.is_lock_free();
    }
};
