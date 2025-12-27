#ifndef MEMORY_ALLOCATOR_TEST_H
#define MEMORY_ALLOCATOR_TEST_H

#include <gtest/gtest.h>
#include "MemoryAllocator.h"
#include <vector>
#include <thread>
#include <random>
#include <algorithm>

class MemoryAllocatorTest : public ::testing::Test {
protected:

    bool isAligned8(void* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 7) == 0;
    }

    void fillMemory(void* ptr, size_t size, unsigned char value) {
        unsigned char* p = static_cast<unsigned char*>(ptr);
        for (size_t i = 0; i < size; ++i) {
            p[i] = value + static_cast<unsigned char>(i % 256);
        }
    }

    bool checkMemory(void* ptr, size_t size, unsigned char value) {
        unsigned char* p = static_cast<unsigned char*>(ptr);
        for (size_t i = 0; i < size; ++i) {
            if (p[i] != static_cast<unsigned char>(value + i % 256)) {
                return false;
            }
        }
        return true;
    }
};

#endif // MEMORY_ALLOCATOR_TEST_H