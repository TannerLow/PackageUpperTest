#include <o2/utils/arena.h>
#include <assert.h>

namespace o2 {
namespace utils {
namespace arena {

    Arena make(void* memory, size_t size) {
        Arena arena;
        arena.base = (uint8_t*)memory;
        arena.size = size;
        arena.used = 0;
        return arena;
    }

    void reset(Arena* arena) {
        arena->used = 0;
    }

    void* push(Arena* arena, size_t size) {
        size_t available = arena->size - arena->used;
        if (size > available) {
            return NULL;
        }

        void* start = (void*)(arena->base + arena->used);
        arena->used += size;

        return start;
    }

    void* pushArray(Arena* arena, size_t sizePerElement, size_t count) {
        // sizePerElement * count must not wrap around
        if (count != 0 && sizePerElement > SIZE_MAX / count) {
            return NULL;
        }

        return push(arena, sizePerElement * count);
    }

    void* pushAligned(Arena* arena, size_t size, size_t alignment) {
        assert(alignment != 0 && (alignment & (alignment - 1)) == 0);

        uintptr_t currentAddress = (uintptr_t)arena->base + arena->used;

        // round up to the next multiple of alignment (alignment must be a power of 2)
        uintptr_t mask = alignment - 1;
        uintptr_t alignedAddress = (currentAddress + mask) & (~mask);
        size_t padding = (size_t)(alignedAddress - currentAddress);

        // compare without adding, so padding + size can't wrap around
        size_t available = arena->size - arena->used;
        if (padding > available || size > available - padding) {
            return NULL;
        }

        arena->used += padding + size;

        return (void*)alignedAddress;
    }

    void* pushArrayAligned(
        Arena* arena,
        size_t sizePerElement,
        size_t count,
        size_t alignment
    ) {
        // sizePerElement * count must not wrap around
        if (count != 0 && sizePerElement > SIZE_MAX / count) {
            return NULL;
        }

        return pushAligned(arena, sizePerElement * count, alignment);
    }

}
}
}
