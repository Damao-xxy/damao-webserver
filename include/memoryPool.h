#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>

namespace memoryPool{
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

struct Slot{
    Slot* next;
};

class MemoryPool{
public:
    MemoryPool(size_t BlockSize = 4096);
    ~MemoryPool();

    void init(size_t);

    void* allocate();
    void deallocate(void*);

private:
    void allocateNewBlock();
    size_t padPointer(char* p, size_t align);

private:
    int BlockSize_;
    int SlotSize_;
    Slot* firstBlock_;  // 指向内存池管理的首个实际内存块
    Slot* curSlot_;  // 指向当前未被使用过的槽
    Slot* freeList_;  // 指向空闲的槽(被使用过后又被释放的槽)
    Slot* lastSlot_;  // 作为当前内存块中最后能够存放元素的位置标识(超过该位置需申请新的内存块)
    std::mutex mutexForFreeList_;  // 保证freeList_在多线程中操作的原子性
    std::mutex mutexForBlock_;  // 保证多线程情况下避免不必要的重复开辟内存导致的浪费行为
};

class HashBucket{
public:
    static void initMemoryPool();
    static MemoryPool& getMemoryPool(int index);

    static void* useMemory(size_t size){
        if(size <= 0) return nullptr;
        if(size > MAX_SLOT_SIZE) return operator new(size);
        return getMemoryPool(((size + 7)/SLOT_BASE_SIZE) - 1).allocate();
    }
    static void freeMemory(void* ptr, size_t size){
        if(!ptr) return;
        if(size > MAX_SLOT_SIZE){
            operator delete(ptr);
            return;
        }

        getMemoryPool(((size + 7)/SLOT_BASE_SIZE) - 1).deallocate(ptr);
    }

    template<typename T, typename... Args>
    friend T* newElement(Args&&... args);

    template<typename T>
    friend void deleteElement(T* p);
};

template<typename T, typename... Args>
T* newElement(Args&&... args){
    T* p = nullptr;
    if((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr)
        new(p) T(std::forward<Args>(args)...);
    
    return p;
}

template<typename T>
void deleteElement(T* p){
    if(p){
        p->~T();
        HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
    }
}
    
}   // namespace memoryPool