#include "memory_pool.h"
#include <stdexcept>

MemoryPool::MemoryPool(size_t blockSize, size_t blockCount) : blockSize_(blockSize), blockCount_(blockCount) {
    if (blockCount_ == 0) {
        throw std::invalid_argument("MemoryPool: blockCount must be > 0");
    }
    grow();
}

MemoryPool::~MemoryPool(){
    for (auto chunk : allChunks_){
        delete[] chunk;
    }
}

void* MemoryPool::allocate(){
    std::lock_guard<std::mutex> lock(mutex_);
    if (freeBlocks_.empty()){
        grow();
    }
    void* block = freeBlocks_.back();
    freeBlocks_.pop_back();
    return block;
}

void MemoryPool::deallocate(void* block){
    std::lock_guard<std::mutex> lock(mutex_);
    freeBlocks_.push_back(static_cast<char*>(block));
}

void MemoryPool::grow(){
    size_t newBlockCount = blockCount_;
    size_t totalSize = blockSize_ * newBlockCount;

    char* newChunk = new char[totalSize];
    allChunks_.push_back(newChunk);

    for (size_t i = 0; i < newBlockCount; ++i){
        char* blockPtr = newChunk + (i * blockSize_);
        freeBlocks_.push_back(blockPtr);
    }

    blockCount_ += newBlockCount;
}