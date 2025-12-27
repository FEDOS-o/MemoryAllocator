#ifndef MEMORY_ALLOCATOR_H
#define MEMORY_ALLOCATOR_H

#include "FixedSizeAllocator.h"
#include "CoalesceAllocator.h"
#include <vector>
#include <iostream>
#include <algorithm>

#ifdef _DEBUG
#define DEBUG_ASSERT(cond) assert(cond)
#else
#define DEBUG_ASSERT(cond)
#endif

class MemoryAllocator final {
private:
    struct OSBlock {
        void* address;
        size_t size;

        OSBlock(void* addr = nullptr, size_t sz = 0) : address(addr), size(sz) {}
    };

    static constexpr size_t FSA_SIZES[] = {16, 32, 64, 128, 256, 512};
    static constexpr size_t NUM_FSA_ALLOCATORS = 6;
    static constexpr size_t FSA_BLOCKS_PER_POOL = 1024;
    static constexpr size_t DIRECT_OS_THRESHOLD = 10 * 1024 * 1024; // 10 МБ

    FixedSizeAllocator* fsaAllocators[NUM_FSA_ALLOCATORS];

    CoalesceAllocator coalesceAllocator;

    std::vector<OSBlock> osBlocks;

    bool initialized;
    bool destroyed;

    static constexpr size_t COALESCE_INITIAL_SIZE = 4 * 1024 * 1024; // 4 МБ начальный размер

    FixedSizeAllocator* findFSAForSize(size_t size) {
        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            if (size <= FSA_SIZES[i] && fsaAllocators[i]) {
                return fsaAllocators[i];
            }
        }
        return nullptr;
    }

    FixedSizeAllocator* findFSAForPointer(void* p) {
        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            if (fsaAllocators[i] && fsaAllocators[i]->belongs(p)) {
                return fsaAllocators[i];
            }
        }
        return nullptr;
    }

    bool isDirectOSAllocation(size_t size) const {
        return size > DIRECT_OS_THRESHOLD;
    }


public:
    MemoryAllocator()
        : initialized(false), destroyed(false) {
        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            fsaAllocators[i] = nullptr;
        }

        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            fsaAllocators[i] = new FixedSizeAllocator(FSA_SIZES[i], FSA_BLOCKS_PER_POOL);
        }
    }

    MemoryAllocator(const MemoryAllocator&) = delete;
    MemoryAllocator& operator=(const MemoryAllocator&) = delete;

    virtual ~MemoryAllocator() {
        DEBUG_ASSERT(destroyed && "MemoryAllocator must be destroyed before destruction");

        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            delete fsaAllocators[i];
            fsaAllocators[i] = nullptr;
        }
    }

    virtual void init() {
        DEBUG_ASSERT(!initialized && "MemoryAllocator already initialized");
        DEBUG_ASSERT(!destroyed && "MemoryAllocator was destroyed");

        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            fsaAllocators[i]->init();
        }

        coalesceAllocator.init(COALESCE_INITIAL_SIZE);

        initialized = true;
        destroyed = false;

        std::cout << "[MemoryAllocator] Initialized with:" << std::endl;
        std::cout << "  - " << NUM_FSA_ALLOCATORS << " FixedSizeAllocators" << std::endl;
        std::cout << "  - CoalesceAllocator with " << COALESCE_INITIAL_SIZE << " bytes" << std::endl;
        std::cout << "  - Direct OS allocations for blocks > " << DIRECT_OS_THRESHOLD << " bytes" << std::endl;
    }

    virtual void destroy() {
        DEBUG_ASSERT(initialized && "MemoryAllocator not initialized");
        DEBUG_ASSERT(!destroyed && "MemoryAllocator already destroyed");

        for (auto& block : osBlocks) {
            free(block.address);
        }
        osBlocks.clear();

        coalesceAllocator.destroy();

        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            fsaAllocators[i]->destroy();
        }

        destroyed = true;
        initialized = false;

        std::cout << "[MemoryAllocator] Destroyed successfully" << std::endl;
    }

    virtual void* alloc(size_t size) {
        DEBUG_ASSERT(initialized && "MemoryAllocator not initialized");
        DEBUG_ASSERT(!destroyed && "MemoryAllocator was destroyed");

        if (size == 0) return nullptr;

        size_t alignedSize = (size + 7) & ~7;

        if (isDirectOSAllocation(alignedSize)) {
            void* ptr = malloc(alignedSize);
            if (!ptr) return nullptr;

            osBlocks.emplace_back(ptr, alignedSize);
            return ptr;
        }

        if (auto* fsa = findFSAForSize(alignedSize)) {
            void* ptr = fsa->alloc();
            if (ptr) return ptr;
        }

        void* ptr = coalesceAllocator.alloc(alignedSize);
        if (ptr) return ptr;

        return nullptr;
    }

    virtual void free(void* p) {
        DEBUG_ASSERT(initialized && "MemoryAllocator not initialized");
        DEBUG_ASSERT(!destroyed && "MemoryAllocator was destroyed");

        if (!p) return;

        auto it = std::find_if(osBlocks.begin(), osBlocks.end(),
            [p](const OSBlock& block) { return block.address == p; });

        if (it != osBlocks.end()) {
            ::free(p);
            osBlocks.erase(it);
            return;
        }

        if (auto* fsa = findFSAForPointer(p)) {
            fsa->free(p);
            return;
        }

        coalesceAllocator.free(p);
    }

    virtual void dumpStat() const {
        DEBUG_ASSERT(initialized && "MemoryAllocator not initialized");
        DEBUG_ASSERT(!destroyed && "MemoryAllocator was destroyed");

        std::cout << "\n=== MemoryAllocator Statistics ===\n";

        std::cout << "\nFixedSizeAllocators:" << std::endl;
        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            std::cout << "  " << FSA_SIZES[i] << " bytes: ";
            fsaAllocators[i]->dumpStat();
        }

        std::cout << "\nCoalesceAllocator:" << std::endl;
        coalesceAllocator.dumpStat();

        std::cout << "\nDirect OS Allocations:" << std::endl;
        std::cout << "  Count: " << osBlocks.size() << std::endl;
        size_t totalOSMemory = 0;
        for (const auto& block : osBlocks) {
            totalOSMemory += block.size;
        }
        std::cout << "  Total memory: " << totalOSMemory << " bytes" << std::endl;

        std::cout << "\nSummary:" << std::endl;
        std::cout << "  FSA sizes: ";
        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            std::cout << FSA_SIZES[i];
            if (i < NUM_FSA_ALLOCATORS - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        std::cout << "  OS threshold: " << DIRECT_OS_THRESHOLD << " bytes" << std::endl;

        std::cout << "=================================\n";
    }

    virtual void dumpBlocks() const {
        DEBUG_ASSERT(initialized && "MemoryAllocator not initialized");
        DEBUG_ASSERT(!destroyed && "MemoryAllocator was destroyed");

        std::cout << "\n=== MemoryAllocator Blocks Dump ===\n";

        std::cout << "\nFixedSizeAllocator blocks:" << std::endl;
        for (size_t i = 0; i < NUM_FSA_ALLOCATORS; ++i) {
            std::cout << "\n" << FSA_SIZES[i] << " bytes blocks:" << std::endl;
            fsaAllocators[i]->dumpBlocks();
        }

        std::cout << "\nCoalesceAllocator blocks:" << std::endl;
        coalesceAllocator.dumpBlocks();

        std::cout << "\nDirect OS Allocations:" << std::endl;
        if (osBlocks.empty()) {
            std::cout << "  No direct OS allocations" << std::endl;
        } else {
            for (size_t i = 0; i < osBlocks.size(); ++i) {
                std::cout << "  Block " << i << ": address=" << osBlocks[i].address
                          << ", size=" << osBlocks[i].size << " bytes" << std::endl;
            }
        }

        std::cout << "\n====================================\n";
    }
};

#endif // MEMORY_ALLOCATOR_H