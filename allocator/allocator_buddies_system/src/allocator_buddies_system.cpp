#include "../include/allocator_buddies_system.h"
#include <stdexcept>
#include <new>

allocator_buddies_system::allocator_buddies_system(
        size_t space_size_power_of_two,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    unsigned char k = __detail::nearest_greater_k_of_2(space_size_power_of_two);

    if (k < min_k)
    {
        throw std::logic_error("Space size is too small");
    }

    size_t space_size = size_t(1) << k;
    size_t total_size = allocator_metadata_size + space_size;

    _trusted_memory = ::operator new(total_size);

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    *reinterpret_cast<fit_mode*>(current_ptr) = allocate_fit_mode;
    current_ptr += sizeof(fit_mode);

    *reinterpret_cast<unsigned char*>(current_ptr) = k;
    current_ptr += sizeof(unsigned char);

    size_t mutex_alignment = alignof(std::mutex);
    size_t current_offset = current_ptr - reinterpret_cast<unsigned char*>(_trusted_memory);
    size_t aligned_offset = (current_offset + mutex_alignment - 1) & ~(mutex_alignment - 1);
    current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory) + aligned_offset;

    new(current_ptr) std::mutex();
    current_ptr += sizeof(std::mutex);

    *reinterpret_cast<size_t*>(current_ptr) = total_size;
    current_ptr += sizeof(size_t);

    void* first_block = current_ptr;

    auto metadata = reinterpret_cast<block_metadata*>(first_block);
    metadata->occupied = false;
    metadata->size = k;
}

allocator_buddies_system::~allocator_buddies_system()
{
    if (_trusted_memory == nullptr)
    {
        return;
    }

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(unsigned char);

    size_t mutex_alignment = alignof(std::mutex);
    size_t current_offset = current_ptr - reinterpret_cast<unsigned char*>(_trusted_memory);
    size_t aligned_offset = (current_offset + mutex_alignment - 1) & ~(mutex_alignment - 1);
    current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory) + aligned_offset;

    reinterpret_cast<std::mutex*>(current_ptr)->~mutex();
    current_ptr += sizeof(std::mutex);

    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);

    ::operator delete(_trusted_memory);
    _trusted_memory = nullptr;
}

allocator_buddies_system::allocator_buddies_system(
    allocator_buddies_system &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_buddies_system &allocator_buddies_system::operator=(
    allocator_buddies_system &&other) noexcept
{
    if (this != &other)
    {
        if (_trusted_memory != nullptr)
        {
            this->~allocator_buddies_system();
        }
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    return *this;
}

[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(
    size_t size)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    auto mode = *reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);

    unsigned char max_k = *reinterpret_cast<unsigned char*>(current_ptr);
    current_ptr += sizeof(unsigned char);

    size_t mutex_alignment = alignof(std::mutex);
    size_t current_offset = current_ptr - reinterpret_cast<unsigned char*>(_trusted_memory);
    size_t aligned_offset = (current_offset + mutex_alignment - 1) & ~(mutex_alignment - 1);
    current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory) + aligned_offset;

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);
    current_ptr += sizeof(std::mutex);
    current_ptr += sizeof(size_t);

    size_t required_size = size + occupied_block_metadata_size;
    unsigned char required_k = __detail::nearest_greater_k_of_2(required_size);

    if (required_k < min_k)
    {
        required_k = min_k;
    }

    if (required_k > max_k)
    {
        throw std::bad_alloc();
    }

    void* best_block = nullptr;
    unsigned char best_k = 0;

    auto memory_start = current_ptr;
    size_t total_space = size_t(1) << max_k;
    auto memory_end = memory_start + total_space;

    void* current_block = memory_start;

    while (reinterpret_cast<unsigned char*>(current_block) < memory_end)
    {
        auto metadata = reinterpret_cast<block_metadata*>(current_block);
        unsigned char block_k = metadata->size;

        if (!metadata->occupied && block_k >= required_k)
        {
            if (mode == fit_mode::first_fit)
            {
                best_block = current_block;
                best_k = block_k;
                break;
            }
            if (mode == fit_mode::the_best_fit)
            {
                if (best_block == nullptr || block_k < best_k)
                {
                    best_block = current_block;
                    best_k = block_k;
                }
            }
            else if (mode == fit_mode::the_worst_fit)
            {
                if (best_block == nullptr || block_k > best_k)
                {
                    best_block = current_block;
                    best_k = block_k;
                }
            }
        }

        size_t block_size = size_t(1) << block_k;
        current_block = reinterpret_cast<unsigned char*>(current_block) + block_size;
    }

    if (best_block == nullptr)
    {
        throw std::bad_alloc();
    }

    while (best_k > required_k)
    {
        best_k--;

        auto metadata = reinterpret_cast<block_metadata*>(best_block);
        metadata->size = best_k;

        size_t half_size = size_t(1) << best_k;
        void* buddy_block = reinterpret_cast<unsigned char*>(best_block) + half_size;

        auto buddy_metadata = reinterpret_cast<block_metadata*>(buddy_block);
        buddy_metadata->occupied = false;
        buddy_metadata->size = best_k;
    }

    auto metadata = reinterpret_cast<block_metadata*>(best_block);
    metadata->occupied = true;
    metadata->size = best_k;

    auto user_ptr = reinterpret_cast<unsigned char*>(best_block) + sizeof(block_metadata);
    *reinterpret_cast<void**>(user_ptr) = _trusted_memory;

    return user_ptr + sizeof(void*);
}

void allocator_buddies_system::do_deallocate_sm(void *at)
{
    if (at == nullptr)
    {
        return;
    }

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);

    unsigned char max_k = *reinterpret_cast<unsigned char*>(current_ptr);
    current_ptr += sizeof(unsigned char);

    size_t mutex_alignment = alignof(std::mutex);
    size_t current_offset = current_ptr - reinterpret_cast<unsigned char*>(_trusted_memory);
    size_t aligned_offset = (current_offset + mutex_alignment - 1) & ~(mutex_alignment - 1);
    current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory) + aligned_offset;

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);
    current_ptr += sizeof(std::mutex);
    current_ptr += sizeof(size_t);

    auto memory_start = current_ptr;
    size_t total_space = size_t(1) << max_k;
    auto memory_end = memory_start + total_space;

    auto block_ptr = reinterpret_cast<unsigned char*>(at);

    if (block_ptr < memory_start || block_ptr >= memory_end)
    {
        throw std::logic_error("Block does not belong to this allocator");
    }

    void* block = block_ptr - occupied_block_metadata_size;

    if (reinterpret_cast<unsigned char*>(block) < memory_start)
    {
        throw std::logic_error("Block does not belong to this allocator");
    }

    auto metadata = reinterpret_cast<block_metadata*>(block);

    if (!metadata->occupied)
    {
        throw std::logic_error("Double free or invalid pointer");
    }

    void* allocator_ptr = *reinterpret_cast<void**>(block_ptr - sizeof(void*));
    if (allocator_ptr != _trusted_memory)
    {
        throw std::logic_error("Block does not belong to this allocator");
    }

    metadata->occupied = false;

    unsigned char block_k = metadata->size;

    while (block_k < max_k)
    {
        size_t block_size = size_t(1) << block_k;
        size_t block_offset = reinterpret_cast<unsigned char*>(block) - memory_start;

        size_t buddy_offset = block_offset ^ block_size;
        void* buddy_block = memory_start + buddy_offset;

        if (buddy_block < memory_start ||
            reinterpret_cast<unsigned char*>(buddy_block) >= memory_end)
        {
            break;
        }

        auto buddy_metadata = reinterpret_cast<block_metadata*>(buddy_block);

        if (buddy_metadata->occupied || buddy_metadata->size != block_k)
        {
            break;
        }

        if (buddy_block < block)
        {
            block = buddy_block;
            metadata = buddy_metadata;
        }

        block_k++;
        metadata->size = block_k;
    }
}

allocator_buddies_system::allocator_buddies_system(const allocator_buddies_system &other)
    : _trusted_memory(nullptr)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(other._trusted_memory);

    auto mode = *reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);

    unsigned char space_size_power = *reinterpret_cast<unsigned char*>(current_ptr);

    *this = allocator_buddies_system(space_size_power, mode);
}

allocator_buddies_system &allocator_buddies_system::operator=(const allocator_buddies_system &other)
{
    if (this != &other)
    {
        this->~allocator_buddies_system();
        new(this) allocator_buddies_system(other);
    }
    return *this;
}

bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    auto other_buddies = dynamic_cast<const allocator_buddies_system*>(&other);
    return other_buddies != nullptr && other_buddies->_trusted_memory == _trusted_memory;
}

void allocator_buddies_system::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    auto mode_ptr = reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(unsigned char);

    size_t mutex_alignment = alignof(std::mutex);
    size_t current_offset = current_ptr - reinterpret_cast<unsigned char*>(_trusted_memory);
    size_t aligned_offset = (current_offset + mutex_alignment - 1) & ~(mutex_alignment - 1);
    current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory) + aligned_offset;

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);

    *mode_ptr = mode;
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(unsigned char);

    size_t mutex_alignment = alignof(std::mutex);
    size_t current_offset = current_ptr - reinterpret_cast<unsigned char*>(_trusted_memory);
    size_t aligned_offset = (current_offset + mutex_alignment - 1) & ~(mutex_alignment - 1);
    current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory) + aligned_offset;

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);

    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> result;

    for (auto it = begin(); it != end(); ++it)
    {
        block_info info;
        info.block_size = it.size();
        info.is_block_occupied = it.occupied();
        result.push_back(info);
    }

    return result;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);

    unsigned char max_k = *reinterpret_cast<unsigned char*>(current_ptr);
    current_ptr += sizeof(unsigned char);

    size_t mutex_alignment = alignof(std::mutex);
    size_t current_offset = current_ptr - reinterpret_cast<unsigned char*>(_trusted_memory);
    size_t aligned_offset = (current_offset + mutex_alignment - 1) & ~(mutex_alignment - 1);
    current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory) + aligned_offset;
    current_ptr += sizeof(std::mutex);
    current_ptr += sizeof(size_t);

    return buddy_iterator(current_ptr, current_ptr, size_t(1) << max_k);
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept
{
    return buddy_iterator();
}

bool allocator_buddies_system::buddy_iterator::operator==(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return _block == other._block;
}

bool allocator_buddies_system::buddy_iterator::operator!=(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept
{
    if (_block == nullptr)
    {
        return *this;
    }

    auto metadata = reinterpret_cast<block_metadata*>(_block);
    size_t block_size = size_t(1) << metadata->size;

    _block = reinterpret_cast<unsigned char*>(_block) + block_size;

    if (reinterpret_cast<unsigned char*>(_block) >= reinterpret_cast<unsigned char*>(_memory_start) + _total_size)
    {
        _block = nullptr;
    }

    return *this;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_buddies_system::buddy_iterator::size() const noexcept
{
    if (_block == nullptr)
    {
        return 0;
    }

    auto metadata = reinterpret_cast<block_metadata*>(_block);
    return size_t(1) << metadata->size;
}

bool allocator_buddies_system::buddy_iterator::occupied() const noexcept
{
    if (_block == nullptr)
    {
        return false;
    }

    auto metadata = reinterpret_cast<block_metadata*>(_block);
    return metadata->occupied;
}

void *allocator_buddies_system::buddy_iterator::operator*() const noexcept
{
    return _block;
}

allocator_buddies_system::buddy_iterator::buddy_iterator(void *start)
    : _block(start), _memory_start(start), _total_size(0)
{
}

allocator_buddies_system::buddy_iterator::buddy_iterator(void *start, void *memory_start, size_t total_size)
    : _block(start), _memory_start(memory_start), _total_size(total_size)
{
}

allocator_buddies_system::buddy_iterator::buddy_iterator()
    : _block(nullptr), _memory_start(nullptr), _total_size(0)
{
}