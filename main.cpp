#include <iostream>
#include "FixedSizeAllocator.h"

int main() {
    std::cout << "=== Testing FixedSizeAllocator ===\n\n";

    std::cout << "1. Creating FixedSizeAllocator (block size: 32, total blocks: 5)\n";
    FixedSizeAllocator allocator(32, 5);

    std::cout << "2. Initializing allocator...\n";
    allocator.init();

    std::cout << "\n3. Initial state:\n";
    allocator.dumpStat();
    allocator.dumpBlocks();

    std::cout << "\n4. Allocating 3 blocks...\n";
    void* block1 = allocator.alloc();
    void* block2 = allocator.alloc();
    void* block3 = allocator.alloc();

    std::cout << "   Block1: " << block1 << "\n";
    std::cout << "   Block2: " << block2 << "\n";
    std::cout << "   Block3: " << block3 << "\n";

    allocator.dumpStat();

    std::cout << "\n5. Writing data to allocated blocks...\n";
    if (block1) *static_cast<int*>(block1) = 100;
    if (block2) *static_cast<int*>(block2) = 200;
    if (block3) *static_cast<int*>(block3) = 300;

    std::cout << "\n6. Freeing block2 (" << block2 << ")...\n";
    allocator.free(block2);
    allocator.dumpStat();
    allocator.dumpBlocks();

    std::cout << "\n7. Allocating one more block...\n";
    void* block4 = allocator.alloc();
    std::cout << "   Block4: " << block4 << " (should be same as block2: " << block2 << ")\n";

    std::cout << "\n8. Trying to allocate remaining blocks...\n";
    void* block5 = allocator.alloc();
    void* block6 = allocator.alloc();
    void* block7 = allocator.alloc();

    std::cout << "   Block5: " << (block5 ? "allocated" : "nullptr") << "\n";
    std::cout << "   Block6: " << (block6 ? "allocated" : "nullptr") << "\n";
    std::cout << "   Block7: " << (block7 ? "allocated" : "nullptr") << "\n";

    std::cout << "\n9. Freeing all blocks...\n";
    if (block1) allocator.free(block1);
    if (block3) allocator.free(block3);
    if (block4) allocator.free(block4);
    if (block5) allocator.free(block5);

    allocator.dumpStat();
    allocator.dumpBlocks();

    std::cout << "\n10. Testing belongs() method:\n";
    std::cout << "    belongs(block1): " << (allocator.belongs(block1) ? "true" : "false") << "\n";
    std::cout << "    belongs(nullptr): " << (allocator.belongs(nullptr) ? "true" : "false") << "\n";

    char outsideMemory[64];
    std::cout << "    belongs(outsideMemory): " << (allocator.belongs(outsideMemory) ? "true" : "false") << "\n";

    std::cout << "\n11. Testing reinitialization...\n";
    allocator.destroy();
    std::cout << "    After destroy:\n";
    allocator.dumpStat();

    allocator.init();
    std::cout << "    After re-init:\n";
    allocator.dumpStat();

    std::cout << "\n12. Final cleanup...\n";
    allocator.destroy();

    std::cout << "\n=== All tests completed ===\n";

    return 0;
}