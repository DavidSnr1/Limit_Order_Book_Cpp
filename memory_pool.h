#pragma once
#include <iostream>
#include <mutex>
#include <vector>
#include "order.h"

class MemoryPool {
    public:
        MemoryPool(size_t blockSize, size_t blockCount);
        ~MemoryPool();

        void* allocate();
        void deallocate(void* block);

    private:
        void grow();

        size_t blockSize_;
        size_t blockCount_;
        std::vector<char*> freeBlocks_;
        std::vector<char*> allChunks_;
        std::mutex mutex_;

};