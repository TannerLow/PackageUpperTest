#pragma once
#include <stddef.h>
#include <stdint.h>

namespace o2 {
namespace arena {

    // Linear allocator over a caller-provided block of memory.
    // Freeing happens all at once with reset.
    struct Arena {
        uint8_t* base;
        size_t size;
        size_t used;
    };

    Arena make(void* memory, size_t size);
    void reset(Arena* arena);

    void* push(Arena* arena, size_t size);
    void* pushArray(Arena* arena, size_t sizePerElement, size_t count);
    void* pushAligned(Arena* arena, size_t size, size_t alignment);
    void* pushArrayAligned(Arena* arena, size_t sizePerElement, size_t count, size_t alignment);

}
}
