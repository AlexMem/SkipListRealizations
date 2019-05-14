#include <atomic>

template<class V> class AtomicMarkableReference {
private:
    static const uintptr_t mask = 1;
    std::atomic<uintptr_t> val{0};
public:
    AtomicMarkableReference() = default;

    AtomicMarkableReference(V* ref, bool mark) {
        val.store(convert(ref, mark));
    }

    V* getRef() {
        return (V*)(val.load(std::memory_order_acquire) & ~mask);
    }

    bool getMark() {
        return static_cast<bool>(val.load(std::memory_order_acquire) & mask);
    }

    uintptr_t getVal() {
        return val.load(std::memory_order_acquire);
    }

    V* getRefAndMark(bool& mark) {
        uintptr_t _val = val.load(std::memory_order_acquire);
        mark = static_cast<bool>(_val & mask);
        return (V*)(_val & ~mask);
    }

    void setVal(V* ref, bool mark) {
        val.store(convert(ref, mark));
    }

    bool CAS(uintptr_t& expected, V* newRef, bool newMark) {
        return val.compare_exchange_strong(expected, convert(newRef, newMark));
    }

    bool CAS(V* expRef, V* newRef, bool expMark, bool newMark) {
        uintptr_t _val = convert(expRef, expMark);
        return val.compare_exchange_strong(_val, convert(newRef, newMark));
    }

    static uintptr_t convert(V* ref, bool mark) {
        return ((uintptr_t)ref & ~mask) | (mark ? mask : 0);
    }

    bool is_lock_free() {
        return val.is_lock_free();
    }
};
