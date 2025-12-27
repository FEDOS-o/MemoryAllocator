#ifndef COALESCE_ALLOCATOR_H
#define COALESCE_ALLOCATOR_H

#include <cstddef>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <cstddef>

class CoalesceAllocator {
public:
    static const size_t ALIGNMENT = 8;

    struct BlockHeader {
        size_t size;
        bool isFree;
        BlockHeader* nextFree; // only if Free
        BlockHeader* prevFree;

        void* getData() {
            size_t offset = sizeof(size_t) + sizeof(bool);
            offset = (offset + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
            return reinterpret_cast<std::byte*>(this) + offset;
        }

        BlockHeader* getNextBlock() {
            return reinterpret_cast<BlockHeader*>(
                reinterpret_cast<std::byte*>(this) + size
            );
        }

        static size_t getOccupiedDataOffset() {
            size_t offset = sizeof(size_t) + sizeof(bool);
            return (offset + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
        }

        static size_t getFreeHeaderSize() {
            return sizeof(BlockHeader);
        }
    };

    struct BlockFooter {
        size_t size;
    };

    static_assert(sizeof(BlockHeader) % ALIGNMENT == 0, "BlockHeader not aligned");
    static_assert(sizeof(BlockFooter) % ALIGNMENT == 0, "BlockFooter not aligned");

    static size_t alignSize(size_t size) {
        return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    static size_t getOccupiedDataOffset() {
        return BlockHeader::getOccupiedDataOffset();
    }

    static size_t getFreeHeaderSize() {
        return BlockHeader::getFreeHeaderSize();
    }

private:
    void* memoryPool;
    size_t poolSize;
    BlockHeader* freeListHead;
    bool initialized;

    BlockHeader* getHeaderFromData(void* data) {
        size_t offset = getOccupiedDataOffset();
        BlockHeader* header = reinterpret_cast<BlockHeader*>(
            static_cast<std::byte*>(data) - offset
        );

        if (!isValidHeader(header) || header->isFree) {
            return nullptr;
        }

        return header;
    }

    bool isValidHeader(BlockHeader* header) const {
        if (!header || !memoryPool) return false;

        std::byte* poolStart = static_cast<std::byte*>(memoryPool);
        std::byte* poolEnd = poolStart + poolSize;
        std::byte* addr = reinterpret_cast<std::byte*>(header);

        if (addr < poolStart || addr >= poolEnd) return false;

        size_t minSize = getOccupiedDataOffset() + sizeof(BlockFooter) + ALIGNMENT;
        if (header->size < minSize) return false;

        if (addr + header->size > poolEnd) return false;

        return true;
    }

    BlockFooter* getFooter(BlockHeader* header) {
        if (!isValidHeader(header)) return nullptr;
        return reinterpret_cast<BlockFooter*>(
            reinterpret_cast<std::byte*>(header) + header->size - sizeof(BlockFooter)
        );
    }

    BlockHeader* getPrevBlock(BlockHeader* header) {
        if (!isValidHeader(header)) return nullptr;

        if (reinterpret_cast<std::byte*>(header) == static_cast<std::byte*>(memoryPool)) {
            return nullptr;
        }

        BlockFooter* prevFooter = reinterpret_cast<BlockFooter*>(
            reinterpret_cast<std::byte*>(header) - sizeof(BlockFooter)
        );

        if (!prevFooter) return nullptr;

        std::byte* prevStart = reinterpret_cast<std::byte*>(header) - prevFooter->size;
        BlockHeader* prev = reinterpret_cast<BlockHeader*>(prevStart);

        return (isValidHeader(prev) && prev->getNextBlock() == header) ? prev : nullptr;
    }

    void removeFromFreeList(BlockHeader* header) {
        if (!header || !header->isFree) return;

        if (!header->prevFree) {
            freeListHead = header->nextFree;
        } else {
            header->prevFree->nextFree = header->nextFree;
        }

        if (header->nextFree) {
            header->nextFree->prevFree = header->prevFree;
        }

        header->nextFree = nullptr;
        header->prevFree = nullptr;
    }

    void addToFreeList(BlockHeader* header) {
        if (!header || !header->isFree) return;

        header->nextFree = freeListHead;
        header->prevFree = nullptr;

        if (freeListHead) {
            freeListHead->prevFree = header;
        }

        freeListHead = header;
    }

    void coalesce(BlockHeader* header) {
        if (!isValidHeader(header)) return;

        BlockHeader* prev = getPrevBlock(header);
        if (prev && prev->isFree) {
            removeFromFreeList(prev);
            prev->size += header->size;
            BlockFooter* footer = getFooter(prev);
            if (footer) footer->size = prev->size;
            header = prev;
        }

        BlockHeader* next = header->getNextBlock();
        if (isValidHeader(next) && next->isFree) {
            removeFromFreeList(next);
            header->size += next->size;
            BlockFooter* footer = getFooter(header);
            if (footer) footer->size = header->size;
        }

        header->isFree = true;
        addToFreeList(header);
    }

public:
    CoalesceAllocator()
        : memoryPool(nullptr), poolSize(0),
          freeListHead(nullptr), initialized(false) {
    }

    ~CoalesceAllocator() {
        destroy();
    }

    void init(size_t size) {
        if (initialized) return;

        size_t minSize = getFreeHeaderSize() + ALIGNMENT + sizeof(BlockFooter);
        poolSize = alignSize(std::max(size, minSize));

        memoryPool = malloc(poolSize);
        if (!memoryPool) throw std::bad_alloc();

        BlockHeader* header = static_cast<BlockHeader*>(memoryPool);
        header->size = poolSize;
        header->isFree = true;
        header->nextFree = nullptr;
        header->prevFree = nullptr;

        BlockFooter* footer = getFooter(header);
        if (footer) footer->size = poolSize;

        freeListHead = header;
        initialized = true;
    }

    void destroy() {
        if (!initialized) return;
        free(memoryPool);
        memoryPool = nullptr;
        poolSize = 0;
        freeListHead = nullptr;
        initialized = false;
    }

    void* alloc(size_t size) {
        assert(initialized && "Allocator not initialized");
        if (size == 0) return nullptr;

        size_t dataSize = alignSize(size);

        size_t occupiedSize = alignSize(getOccupiedDataOffset() + dataSize + sizeof(BlockFooter));

        BlockHeader* current = freeListHead;
        BlockHeader* bestFit = nullptr;

        while (current) {
            if (current->isFree) {
                if (current->size >= occupiedSize) {
                    bestFit = current;
                    break;
                }
            }
            current = current->nextFree;
        }

        if (!bestFit) return nullptr;

        removeFromFreeList(bestFit);

        size_t remaining = bestFit->size - occupiedSize;

        size_t minFreeSize = getFreeHeaderSize() + ALIGNMENT + sizeof(BlockFooter);

        if (remaining >= minFreeSize) {
            bestFit->size = occupiedSize;
            bestFit->isFree = false;

            BlockFooter* footer = getFooter(bestFit);
            if (footer) footer->size = occupiedSize;

            BlockHeader* newBlock = bestFit->getNextBlock();
            newBlock->size = remaining;
            newBlock->isFree = true;
            newBlock->nextFree = nullptr;
            newBlock->prevFree = nullptr;

            BlockFooter* newFooter = getFooter(newBlock);
            if (newFooter) newFooter->size = remaining;

            addToFreeList(newBlock);
        } else {
            bestFit->isFree = false;
        }

        return bestFit->getData();
    }

    void free(void* ptr) {
        assert(initialized && "Allocator not initialized");
        if (!ptr) return;

        BlockHeader* header = getHeaderFromData(ptr);
        if (!header) {
            return;
        }

        if (header->isFree) {
            assert(false && "Double free detected");
            return;
        }

        coalesce(header);
    }

    void dumpStat() const {
        if (!initialized) {
            std::cout << "[CoalesceAllocator] Not initialized.\n";
            return;
        }

        std::cout << "\n=== Coalesce Allocator Statistics ===\n";
        std::cout << "Pool: " << poolSize << " bytes at " << memoryPool << "\n";

        size_t freeBlocks = 0;
        size_t freeMemory = 0;
        BlockHeader* current = freeListHead;

        while (current && freeBlocks < 1000) {
            freeBlocks++;
            size_t userSize = current->size - getFreeHeaderSize() - sizeof(BlockFooter);
            freeMemory += userSize;
            current = current->nextFree;
        }

        std::cout << "Free blocks in list: " << freeBlocks << "\n";
        std::cout << "Free memory: " << freeMemory << " bytes ("
                  << (freeMemory * 100.0 / poolSize) << "%)\n";

        BlockHeader* block = static_cast<BlockHeader*>(memoryPool);
        std::byte* poolEnd = static_cast<std::byte*>(memoryPool) + poolSize;
        size_t totalBlocks = 0;
        size_t usedBlocks = 0;
        size_t usedMemory = 0;

        while (reinterpret_cast<std::byte*>(block) < poolEnd && totalBlocks < 1000) {
            totalBlocks++;

            if (!block->isFree) {
                usedBlocks++;
                size_t userSize = block->size - getOccupiedDataOffset() - sizeof(BlockFooter);
                usedMemory += userSize;
            }

            block = block->getNextBlock();
        }

        std::cout << "Total blocks in memory: " << totalBlocks << "\n";
        std::cout << "Used blocks: " << usedBlocks << "\n";
        std::cout << "Used memory: " << usedMemory << " bytes ("
                  << (usedMemory * 100.0 / poolSize) << "%)\n";

        std::cout << "=====================================\n";
    }

    void dumpBlocks() const {
        if (!initialized) {
            std::cout << "[CoalesceAllocator] Not initialized.\n";
            return;
        }

        std::cout << "\n=== Coalesce Allocator Blocks Dump ===\n";
        std::cout << "Memory pool: " << memoryPool << " - "
                  << static_cast<std::byte*>(memoryPool) + poolSize
                  << " (" << poolSize << " bytes)\n\n";

        std::cout << "Block#  Address        Status    Size     Type        Data Start\n";
        std::cout << "------------------------------------------------------------------\n";

        BlockHeader* block = static_cast<BlockHeader*>(memoryPool);
        std::byte* poolStart = static_cast<std::byte*>(memoryPool);
        std::byte* poolEnd = poolStart + poolSize;
        size_t blockNum = 0;

        while (reinterpret_cast<std::byte*>(block) < poolEnd && blockNum < 100) {
            if (reinterpret_cast<std::byte*>(block) < poolStart ||
                reinterpret_cast<std::byte*>(block) >= poolEnd) {
                std::cout << "ERROR: Block outside pool!\n";
                break;
            }

            if (block->size == 0 || block->size > poolSize) {
                std::cout << "ERROR: Invalid block size: " << block->size << "\n";
                break;
            }

            std::cout << std::setw(6) << blockNum++ << "  "
                      << std::setw(14) << block << "  "
                      << (block->isFree ? "FREE " : "USED ") << "  "
                      << std::setw(6) << block->size << "  "
                      << (block->isFree ? "FREE_HDR" : "OCCUPIED") << "  ";

            if (!block->isFree) {
                std::cout << block->getData();
            } else {
                std::cout << "-";
            }

            std::cout << "\n";

            BlockHeader* nextBlock = block->getNextBlock();
            if (nextBlock <= block) {
                std::cout << "ERROR: Next block not after current!\n";
                break;
            }

            block = nextBlock;
        }

        std::cout << "=========================================\n";
    }
};

#endif //COALESCE_ALLOCATOR_H