#include <o2/gui/arena.h>
#include <assert.h>

namespace o2 {
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
        assert(arena->used + size <= arena->size);

        void* start = (void*)(arena->base + arena->used);
        arena->used += size;

        return start;
    }

    void* pushArray(Arena* arena, size_t sizePerElement, size_t count) {
        return push(arena, sizePerElement * count);
    }

    void* pushAligned(Arena* arena, size_t size, size_t alignment) {
        uintptr_t currentAddress = (uintptr_t)arena->base + arena->used;

        // round up to the next multiple of alignment (alignment must be a power of 2)
        uintptr_t mask = alignment - 1;
        uintptr_t alignedAddress = (currentAddress + mask) & (~mask);
        size_t totalSize = alignedAddress + size - currentAddress;

        assert(arena->used + totalSize <= arena->size);

        arena->used += totalSize;

        return (void*)alignedAddress;
    }

    void* pushArrayAligned(Arena* arena, size_t sizePerElement, size_t count, size_t alignment) {
        return pushAligned(arena, sizePerElement * count, alignment);
    }

}
}
