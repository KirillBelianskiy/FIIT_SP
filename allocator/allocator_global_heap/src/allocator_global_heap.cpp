#include <not_implemented.h>
#include "../include/allocator_global_heap.h"

namespace {
    allocator_global_heap& get_default_allocator() {
        static allocator_global_heap instance;
        return instance;
    }
}

allocator_global_heap& allocator_global_heap::get_instance() {
    return get_default_allocator();
}

[[nodiscard]] void* allocator_global_heap::do_allocate_sm(size_t size)
{
        std::lock_guard<std::mutex> lock(_mutex);
        return ::operator new(size);
}

void allocator_global_heap::do_deallocate_sm(
    void* at)
{
    std::lock_guard<std::mutex> lock(_mutex);
    ::operator delete(at);
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource& other) const noexcept
{
    return dynamic_cast<const allocator_global_heap*>(&other) != nullptr;
}
