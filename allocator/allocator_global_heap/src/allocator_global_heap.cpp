#include <not_implemented.h>
#include "../include/allocator_global_heap.h"

[[nodiscard]] void* allocator_global_heap::do_allocate_sm(
    size_t size)
{
    try
    {
        std::lock_guard<std::mutex> lock(_mutex);

        return ::operator new(size);
    }
    catch (const std::bad_alloc&)
    {
        throw std::bad_alloc();
    }
}

void allocator_global_heap::do_deallocate_sm(
    void* at)
{
    std::lock_guard<std::mutex> lock(_mutex);

    ::operator delete(at);
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource& other) const noexcept
{
    return this == &other;
}
