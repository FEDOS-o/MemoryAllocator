#ifndef FIXED_SIZE_ALLOCATOR_H
#define FIXED_SIZE_ALLOCATOR_H

#include <cstddef>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <vector>

class FixedSizeAllocator {
public:
    FixedSizeAllocator()
        : m_blockSize(0), m_totalBlocks(0),
          m_memoryStart(nullptr), m_freeListHead(nullptr),
          m_initialized(false) {
    }

    FixedSizeAllocator(size_t blockSize, size_t totalBlocks)
        : m_blockSize((blockSize + 7) & ~7)
          , m_totalBlocks(totalBlocks)
          , m_memoryStart(nullptr)
          , m_freeListHead(nullptr)
          , m_initialized(false) {
        assert(blockSize >= 8 && "Block size must be at least 8 bytes");
        assert(totalBlocks > 0 && "Total blocks must be positive");
    }

    ~FixedSizeAllocator() {
        destroy();
    }

    FixedSizeAllocator(const FixedSizeAllocator &) = delete;

    FixedSizeAllocator &operator=(const FixedSizeAllocator &) = delete;

    void init() {
        if (m_initialized) return;

        m_memoryStart = malloc(m_blockSize * m_totalBlocks);

        m_freeListHead = static_cast<size_t *>(m_memoryStart);
        size_t *current = m_freeListHead;

        for (size_t i = 0; i < m_totalBlocks; ++i) {
            size_t *next = current + m_blockSize / sizeof(size_t);
            *current = i + 1;
            current = next;
        }
        //last block has next index = m_totalBlocks
        m_initialized = true;
    }

    void destroy() {
        if (!m_initialized) return;

        free(m_memoryStart);
        m_memoryStart = nullptr;
        m_freeListHead = nullptr;
        m_initialized = false;
    }

    void *alloc() {
        assert(m_initialized && "Allocator not initialized!");

        if (m_freeListHead == nullptr) {
            return nullptr;
        }

        void *block = m_freeListHead;

        size_t next = *m_freeListHead;
        if (next == m_totalBlocks) {
            m_freeListHead = nullptr;
        } else {
            m_freeListHead = static_cast<size_t *>(m_memoryStart) + m_blockSize / sizeof(size_t) * next;
        }
        return block;
    }

    void free(void *p) {
        assert(m_initialized && "Allocator not initialized!");
        assert(belongs(p) && "Pointer does not belong to this allocator!");

        size_t index;
        if (m_freeListHead == nullptr) {
            index = m_totalBlocks;
        } else {
            index = (reinterpret_cast<std::byte *>(m_freeListHead) - static_cast<std::byte *>(m_memoryStart)) /
                    m_blockSize;
        }
        auto pointer = static_cast<size_t *>(p);
        *pointer = index;
        m_freeListHead = pointer;
    }

    bool belongs(void *p) const {
        if (!m_initialized) return false;

        auto start = static_cast<std::byte *>(m_memoryStart);
        auto ptr = static_cast<std::byte *>(p);

        if (ptr < start) return false;

        size_t offset = ptr - start;

        return (offset < m_blockSize * m_totalBlocks) &&
               (offset % m_blockSize == 0);
    }

    bool hasFreeBlocks() const {
        return m_freeListHead != nullptr;
    }

    size_t getBlockSize() const {
        return m_blockSize;
    }

    size_t getTotalBlocks() const {
        return m_totalBlocks;
    }


    size_t getFreeBlocksCount() const {
        if (!m_initialized || !m_freeListHead) return 0;

        size_t count = 1;
        size_t *current = m_freeListHead;
        while (*current != m_totalBlocks) {
            ++count;
            current = static_cast<size_t *>(m_memoryStart) + m_blockSize / sizeof(size_t) * (*current);
        }
        return count;
    }

    size_t getUsedBlocksCount() const {
        return m_totalBlocks - getFreeBlocksCount();
    }

    bool isInitialized() const {
        return m_initialized;
    }

    void dumpStat() const {
        if (!m_initialized) {
            std::cout << "[FixedSizeAllocator] Not initialized.\n";
            return;
        }

        size_t freeCount = getFreeBlocksCount();

        std::cout << "[FixedSizeAllocator] Block size: " << m_blockSize
                << ", Total blocks: " << m_totalBlocks
                << ", Free: " << freeCount
                << ", Used: " << (m_totalBlocks - freeCount)
                << ", Memory range: [" << m_memoryStart
                << " - " << static_cast<std::byte *>(m_memoryStart) + m_blockSize * m_totalBlocks
                << ")\n";
    }

    void dumpBlocks() const {
        if (!m_initialized) {
            std::cout << "[FixedSizeAllocator] Not initialized.\n";
            return;
        }

        std::cout << "[FixedSizeAllocator] Memory dump:\n";
        std::cout << "  Start address: " << m_memoryStart << "\n";
        std::cout << "  Block size: " << m_blockSize << " bytes\n";
        std::cout << "  Total blocks: " << m_totalBlocks << "\n";
        std::cout << "  Total memory: " << (m_blockSize * m_totalBlocks) << " bytes\n\n";

        std::vector<bool> isFree(m_totalBlocks, false);

        if (m_freeListHead != nullptr) {
            size_t *current = m_freeListHead;
            size_t visited = 0;

            while (current != nullptr && visited < m_totalBlocks) {
                std::byte *memStart = static_cast<std::byte *>(m_memoryStart);
                std::byte *currByte = reinterpret_cast<std::byte *>(current);
                size_t index = (currByte - memStart) / m_blockSize;

                if (index < m_totalBlocks) {
                    isFree[index] = true;
                }

                size_t nextIndex = *current;
                if (nextIndex == m_totalBlocks) {
                    break;
                }

                size_t elementsPerBlock = m_blockSize / sizeof(size_t);
                current = static_cast<size_t *>(m_memoryStart) + elementsPerBlock * nextIndex;
                visited++;
            }
        }

        std::cout << "  Block#  Address        Status    Next\n";
        std::cout << "  --------------------------------------\n";

        std::byte *memStart = static_cast<std::byte *>(m_memoryStart);
        for (size_t i = 0; i < m_totalBlocks; ++i) {
            void *addr = memStart + i * m_blockSize;
            size_t *blockPtr = static_cast<size_t *>(addr);

            std::cout << "  " << std::setw(6) << i << "  "
                    << std::setw(14) << addr << "  ";

            if (isFree[i]) {
                std::cout << "FREE      ";
                size_t next = *blockPtr;
                if (next == m_totalBlocks) {
                    std::cout << "END";
                } else {
                    std::cout << "-> Block " << next;
                }
            } else {
                std::cout << "USED      -";
            }
            std::cout << "\n";
        }
    }

private:
    size_t m_blockSize;
    size_t m_totalBlocks;
    void *m_memoryStart;
    size_t *m_freeListHead;
    bool m_initialized;
};

#endif // FIXED_SIZE_ALLOCATOR_H
