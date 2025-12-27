#include "memory_allocator_test.h"

TEST_F(MemoryAllocatorTest, MemoryAllocator_Initialization) {
    MemoryAllocator allocator;

    allocator.init();

    EXPECT_NO_THROW(allocator.dumpStat());
    EXPECT_NO_THROW(allocator.dumpBlocks());

    allocator.destroy();
}

TEST_F(MemoryAllocatorTest, MemoryAllocator_SmallAllocations_FSA) {
    MemoryAllocator allocator;
    allocator.init();

    std::vector<void*> pointers;

    void* p16 = allocator.alloc(10); // Меньше 16
    EXPECT_NE(p16, nullptr);
    pointers.push_back(p16);

    void* p32 = allocator.alloc(25);
    EXPECT_NE(p32, nullptr);
    pointers.push_back(p32);

    void* p64 = allocator.alloc(50);
    EXPECT_NE(p64, nullptr);
    pointers.push_back(p64);

    void* p128 = allocator.alloc(100);
    EXPECT_NE(p128, nullptr);
    pointers.push_back(p128);

    void* p256 = allocator.alloc(200);
    EXPECT_NE(p256, nullptr);
    pointers.push_back(p256);

    void* p512 = allocator.alloc(400);
    EXPECT_NE(p512, nullptr);
    pointers.push_back(p512);

    for (void* ptr : pointers) {
        EXPECT_TRUE(isAligned8(ptr));
    }

    for (size_t i = 0; i < pointers.size(); ++i) {
        size_t size = 10 * (1 << i);
        fillMemory(pointers[i], size, static_cast<unsigned char>(i));
        EXPECT_TRUE(checkMemory(pointers[i], size, static_cast<unsigned char>(i)));
    }

    for (void* ptr : pointers) {
        allocator.free(ptr);
    }

    allocator.destroy();
}

TEST_F(MemoryAllocatorTest, MemoryAllocator_MediumAllocations_Coalesce) {
    MemoryAllocator allocator;
    allocator.init();

    std::vector<void*> pointers;

    void* p1 = allocator.alloc(600);
    void* p2 = allocator.alloc(1000);
    void* p3 = allocator.alloc(2048);

    EXPECT_NE(p1, nullptr);
    EXPECT_NE(p2, nullptr);
    EXPECT_NE(p3, nullptr);
    EXPECT_TRUE(isAligned8(p1));
    EXPECT_TRUE(isAligned8(p2));
    EXPECT_TRUE(isAligned8(p3));

    pointers.push_back(p1);
    pointers.push_back(p2);
    pointers.push_back(p3);

    for (size_t i = 0; i < pointers.size(); ++i) {
        size_t size = 600 * (i + 1);
        fillMemory(pointers[i], size, static_cast<unsigned char>(i + 100));
        EXPECT_TRUE(checkMemory(pointers[i], size, static_cast<unsigned char>(i + 100)));
    }

    for (void* ptr : pointers) {
        allocator.free(ptr);
    }

    allocator.destroy();
}

TEST_F(MemoryAllocatorTest, MemoryAllocator_LargeAllocations_OS) {
    MemoryAllocator allocator;
    allocator.init();

    void* p1 = allocator.alloc(11 * 1024 * 1024);
    void* p2 = allocator.alloc(20 * 1024 * 1024);

    EXPECT_NE(p1, nullptr);
    EXPECT_NE(p2, nullptr);
    EXPECT_TRUE(isAligned8(p1));
    EXPECT_TRUE(isAligned8(p2));

    EXPECT_NE(p1, p2);

    fillMemory(p1, 1024, 0xAA);
    fillMemory(p2, 1024, 0xBB);
    EXPECT_TRUE(checkMemory(p1, 1024, 0xAA));
    EXPECT_TRUE(checkMemory(p2, 1024, 0xBB));

    allocator.free(p1);
    allocator.free(p2);

    allocator.destroy();
}

TEST_F(MemoryAllocatorTest, MemoryAllocator_MixedAllocations) {
    MemoryAllocator allocator;
    allocator.init();

    std::vector<void*> pointers;
    std::vector<size_t> sizes = {
        10,     // FSA 16
        30,     // FSA 32
        60,     // FSA 64
        150,    // FSA 128
        300,    // FSA 256
        500,    // FSA 512
        600,    // Coalesce
        5000,   // Coalesce
        1024*1024, // Coalesce (1MB)
        11*1024*1024 // OS (11MB)
    };

    for (size_t size : sizes) {
        void* ptr = allocator.alloc(size);
        EXPECT_NE(ptr, nullptr);
        EXPECT_TRUE(isAligned8(ptr));
        pointers.push_back(ptr);

        fillMemory(ptr, std::min(size, static_cast<size_t>(1024)),
                  static_cast<unsigned char>(size % 256));
    }

    EXPECT_NO_THROW(allocator.dumpStat());

    for (size_t i = 0; i < pointers.size(); ++i) {
        size_t checkSize = std::min(sizes[i], static_cast<size_t>(1024));
        EXPECT_TRUE(checkMemory(pointers[i], checkSize,
                               static_cast<unsigned char>(sizes[i] % 256)));
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(pointers.begin(), pointers.end(), g);

    for (void* ptr : pointers) {
        allocator.free(ptr);
    }

    EXPECT_NO_THROW(allocator.dumpStat());

    allocator.destroy();
}


TEST_F(MemoryAllocatorTest, MemoryAllocator_ZeroSize) {
    MemoryAllocator allocator;
    allocator.init();

    void* ptr = allocator.alloc(0);
    EXPECT_EQ(ptr, nullptr);

    allocator.destroy();
}

TEST_F(MemoryAllocatorTest, MemoryAllocator_InvalidFree) {
    MemoryAllocator allocator;
    allocator.init();

    int dummy;
    void* invalid_ptr = &dummy;

#ifdef _DEBUG
    EXPECT_DEATH(allocator.free(invalid_ptr), ".*");
#endif

    allocator.destroy();
}

