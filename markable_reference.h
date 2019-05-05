#ifndef CONCURRENTSKIPLIST_MARKABLE_REFERENCE_H
#define CONCURRENTSKIPLIST_MARKABLE_REFERENCE_H

#endif //CONCURRENTSKIPLIST_MARKABLE_REFERENCE_H

template<class V> class MarkableReference {
private:
    static const uintptr_t mask = 0b00000000000000000000000000000001;
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

    bool operator!=(const MarkableReference& other) {
        return val!=other.val;
    }
};