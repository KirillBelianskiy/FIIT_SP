#include "../include/allocator_boundary_tags.h"
#include <stdexcept>

allocator_boundary_tags::allocator_boundary_tags(
    size_t space_size,
    allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < occupied_block_metadata_size)
    {
        throw std::bad_alloc();
    }

    size_t total_size = allocator_metadata_size + space_size;

    _trusted_memory = ::operator new(total_size);

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    *reinterpret_cast<fit_mode*>(current_ptr) = allocate_fit_mode;
    current_ptr += sizeof(fit_mode);

    *reinterpret_cast<size_t*>(current_ptr) = total_size;
    current_ptr += sizeof(size_t);

    new(current_ptr) std::mutex();
    current_ptr += sizeof(std::mutex);

    void* first_block = current_ptr + sizeof(void*);
    *reinterpret_cast<void**>(current_ptr) = first_block;
    current_ptr += sizeof(void*);

    auto first_block_ptr = reinterpret_cast<unsigned char*>(first_block);
    *reinterpret_cast<size_t*>(first_block_ptr) = space_size;
    *reinterpret_cast<bool*>(first_block_ptr + sizeof(size_t)) = false;
    *reinterpret_cast<void**>(first_block_ptr + sizeof(size_t) + sizeof(bool)) = nullptr;
    *reinterpret_cast<void**>(first_block_ptr + sizeof(size_t) + sizeof(bool) + sizeof(void*)) = nullptr;

    auto footer_ptr = first_block_ptr + space_size - sizeof(size_t);
    *reinterpret_cast<size_t*>(footer_ptr) = space_size;
}

allocator_boundary_tags::~allocator_boundary_tags()
{
    if (_trusted_memory == nullptr)
    {
        return;
    }

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);

    reinterpret_cast<std::mutex*>(current_ptr)->~mutex();

    ::operator delete(_trusted_memory);
    _trusted_memory = nullptr;
}

allocator_boundary_tags::allocator_boundary_tags(
    allocator_boundary_tags&& other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_boundary_tags& allocator_boundary_tags::operator=(
    allocator_boundary_tags&& other) noexcept
{
    if (this != &other)
    {
        if (_trusted_memory != nullptr)
        {
            this->~allocator_boundary_tags();
        }
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    return *this;
}

[[nodiscard]] void* allocator_boundary_tags::do_allocate_sm(
    size_t size)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    auto mode = *reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);
    current_ptr += sizeof(std::mutex);

    auto first_free_ptr = reinterpret_cast<void**>(current_ptr);

    size_t required_size = size + occupied_block_metadata_size;

    void* best_block = nullptr;
    void** best_prev = nullptr;
    size_t best_size = 0;

    void** prev_ptr = first_free_ptr;
    void* current_block = *first_free_ptr;

    while (current_block != nullptr)
    {
        size_t block_size = *reinterpret_cast<size_t*>(current_block);

        if (block_size >= required_size)
        {
            if (mode == fit_mode::first_fit)
            {
                best_block = current_block;
                best_prev = prev_ptr;
                best_size = block_size;
                break;
            }
            else if (mode == fit_mode::the_best_fit)
            {
                if (best_block == nullptr || block_size < best_size)
                {
                    best_block = current_block;
                    best_prev = prev_ptr;
                    best_size = block_size;
                }
            }
            else if (mode == fit_mode::the_worst_fit)
            {
                if (best_block == nullptr || block_size > best_size)
                {
                    best_block = current_block;
                    best_prev = prev_ptr;
                    best_size = block_size;
                }
            }
        }

        prev_ptr = reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(current_block) + sizeof(size_t) + sizeof(bool));
        current_block = *prev_ptr;
    }

    if (best_block == nullptr)
    {
        throw std::bad_alloc();
    }

    size_t remaining_size = best_size - required_size;

    if (remaining_size >= occupied_block_metadata_size)
    {
        void* new_free_block = reinterpret_cast<unsigned char*>(best_block) + required_size;
        void* next_free = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(best_block) + sizeof(size_t) + sizeof(bool));

        auto new_block_ptr = reinterpret_cast<unsigned char*>(new_free_block);
        *reinterpret_cast<size_t*>(new_block_ptr) = remaining_size;
        *reinterpret_cast<bool*>(new_block_ptr + sizeof(size_t)) = false;
        *reinterpret_cast<void**>(new_block_ptr + sizeof(size_t) + sizeof(bool)) = next_free;
        *reinterpret_cast<void**>(new_block_ptr + sizeof(size_t) + sizeof(bool) + sizeof(void*)) = nullptr;

        auto new_footer_ptr = new_block_ptr + remaining_size - sizeof(size_t);
        *reinterpret_cast<size_t*>(new_footer_ptr) = remaining_size;

        *best_prev = new_free_block;
    }
    else
    {
        *best_prev = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(best_block) + sizeof(size_t) + sizeof(bool));
        required_size = best_size;
    }

    auto best_block_ptr = reinterpret_cast<unsigned char*>(best_block);
    *reinterpret_cast<size_t*>(best_block_ptr) = required_size;
    *reinterpret_cast<bool*>(best_block_ptr + sizeof(size_t)) = true;  // занят
    *reinterpret_cast<void**>(best_block_ptr + sizeof(size_t) + sizeof(bool)) = nullptr;

    auto footer_ptr = best_block_ptr + required_size - sizeof(size_t);
    *reinterpret_cast<size_t*>(footer_ptr) = required_size;

    if (required_size >= occupied_block_metadata_size + sizeof(size_t))
    {
        *reinterpret_cast<void**>(best_block_ptr + sizeof(size_t) + sizeof(bool) + sizeof(void*)) = _trusted_memory;
    }

    return reinterpret_cast<unsigned char*>(best_block) + occupied_block_metadata_size;
}

void allocator_boundary_tags::do_deallocate_sm(
    void* at)
{
    if (at == nullptr)
    {
        return;
    }

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);

    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);
    current_ptr += sizeof(size_t);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);
    current_ptr += sizeof(std::mutex);

    auto memory_start = reinterpret_cast<unsigned char*>(_trusted_memory) + allocator_metadata_size;
    auto memory_end = reinterpret_cast<unsigned char*>(_trusted_memory) + total_size;
    auto block_ptr = reinterpret_cast<unsigned char*>(at);

    void* block = block_ptr - occupied_block_metadata_size;

    if (reinterpret_cast<unsigned char*>(block) < memory_start ||
        reinterpret_cast<unsigned char*>(block) >= memory_end)
    {
        throw std::logic_error("Block does not belong to this allocator");
    }

    auto block_data_ptr = reinterpret_cast<unsigned char*>(block);
    bool is_occupied = *reinterpret_cast<bool*>(block_data_ptr + sizeof(size_t));

    if (!is_occupied)
    {
        throw std::logic_error("Double free or invalid pointer");
    }

    size_t block_size = *reinterpret_cast<size_t*>(block);

    if (block_size >= occupied_block_metadata_size + sizeof(size_t))
    {
        void* allocator_ptr = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(block) + sizeof(size_t) +
            sizeof(bool) + sizeof(void*));
        if (allocator_ptr != _trusted_memory)
        {
            throw std::logic_error("Block does not belong to this allocator");
        }
    }
    auto block_start = reinterpret_cast<unsigned char*>(block);
    auto block_end = block_start + block_size;

    auto first_block_start = reinterpret_cast<unsigned char*>(_trusted_memory) + allocator_metadata_size;
    auto last_block_end = memory_end;

    void* prev_block = nullptr;
    void* next_block = nullptr;

    if (block_start > first_block_start)
    {
        auto prev_footer_ptr = block_start - sizeof(size_t);
        size_t prev_size = *reinterpret_cast<size_t*>(prev_footer_ptr);
        prev_block = block_start - prev_size;

        auto prev_block_ptr = reinterpret_cast<unsigned char*>(prev_block);
        bool prev_is_occupied = *reinterpret_cast<bool*>(prev_block_ptr + sizeof(size_t));

        if (!prev_is_occupied)
        {
            block_size += prev_size;
            block = prev_block;
            block_start = reinterpret_cast<unsigned char*>(block);
        }
        else
        {
            prev_block = nullptr;
        }
    }

    if (block_end < last_block_end)
    {
        next_block = block_end;
        size_t next_size = *reinterpret_cast<size_t*>(next_block);

        auto next_block_ptr = reinterpret_cast<unsigned char*>(next_block);
        bool next_is_occupied = *reinterpret_cast<bool*>(next_block_ptr + sizeof(size_t));

        if (!next_is_occupied)
        {
            block_size += next_size;
        }
        else
        {
            next_block = nullptr;
        }
    }

    auto first_free_ptr = reinterpret_cast<void**>(current_ptr);

    if (next_block != nullptr)
    {
        void** prev_ptr = first_free_ptr;
        void* current_free = *first_free_ptr;

        while (current_free != nullptr && current_free != next_block)
        {
            prev_ptr = reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(current_free) + sizeof(size_t) + sizeof(bool));
            current_free = *prev_ptr;
        }

        if (current_free == next_block)
        {
            void* next_free = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(next_block) + sizeof(size_t) + sizeof(bool));
            *prev_ptr = next_free;
        }
    }

    if (prev_block != nullptr)
    {
        void** prev_ptr = first_free_ptr;
        void* current_free = *first_free_ptr;

        while (current_free != nullptr && current_free != prev_block)
        {
            prev_ptr = reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(current_free) + sizeof(size_t) + sizeof(bool));
            current_free = *prev_ptr;
        }

        if (current_free == prev_block)
        {
            void* next_free = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(prev_block) + sizeof(size_t) + sizeof(bool));
            *prev_ptr = next_free;
        }
    }

    *reinterpret_cast<size_t*>(block) = block_size;

    void** prev_ptr = first_free_ptr;
    void* current_free = *first_free_ptr;

    while (current_free != nullptr && current_free < block)
    {
        prev_ptr = reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(current_free) + sizeof(size_t) + sizeof(bool));
        current_free = *prev_ptr;
    }

    auto block_ptr2 = reinterpret_cast<unsigned char*>(block);
    *reinterpret_cast<bool*>(block_ptr2 + sizeof(size_t)) = false;
    *reinterpret_cast<void**>(block_ptr2 + sizeof(size_t) + sizeof(bool)) = current_free;
    *reinterpret_cast<void**>(block_ptr2 + sizeof(size_t) + sizeof(bool) + sizeof(void*)) = nullptr;
    *prev_ptr = block;

    auto footer_ptr = block_ptr2 + block_size - sizeof(size_t);
    *reinterpret_cast<size_t*>(footer_ptr) = block_size;
}

inline void allocator_boundary_tags::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    auto mode_ptr = reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);

    *mode_ptr = mode;
}


std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);

    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const
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

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept
{
    return boundary_iterator(_trusted_memory);
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept
{
    return boundary_iterator();
}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags& other)
    : _trusted_memory(nullptr)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(other._trusted_memory);

    auto mode = *reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);

    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);

    size_t space_size = total_size - allocator_metadata_size;

    *this = allocator_boundary_tags(space_size, mode);
}

allocator_boundary_tags& allocator_boundary_tags::operator=(const allocator_boundary_tags& other)
{
    if (this != &other)
    {
        this->~allocator_boundary_tags();
        new(this) allocator_boundary_tags(other);
    }
    return *this;
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource& other) const noexcept
{
    auto other_boundary = dynamic_cast<const allocator_boundary_tags*>(&other);
    return other_boundary != nullptr && other_boundary->_trusted_memory == _trusted_memory;
}

allocator_boundary_tags::boundary_iterator::boundary_iterator()
    : _occupied_ptr(nullptr), _occupied(false), _trusted_memory(nullptr)
{
}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void* trusted)
    : _trusted_memory(trusted)
{
    if (trusted == nullptr)
    {
        _occupied_ptr = nullptr;
        _occupied = false;
        return;
    }

    auto current_ptr = reinterpret_cast<unsigned char*>(trusted);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);
    current_ptr += sizeof(std::mutex);
    current_ptr += sizeof(void*);

    _occupied_ptr = current_ptr;
    auto block_ptr = reinterpret_cast<unsigned char*>(_occupied_ptr);
    _occupied = *reinterpret_cast<bool*>(block_ptr + sizeof(size_t));
}

bool allocator_boundary_tags::boundary_iterator::operator==(
    const allocator_boundary_tags::boundary_iterator& other) const noexcept
{
    return _occupied_ptr == other._occupied_ptr;
}

bool allocator_boundary_tags::boundary_iterator::operator!=(
    const allocator_boundary_tags::boundary_iterator& other) const noexcept
{
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator& allocator_boundary_tags::boundary_iterator::operator++() & noexcept
{
    if (_occupied_ptr == nullptr)
    {
        return *this;
    }

    size_t block_size = *reinterpret_cast<size_t*>(_occupied_ptr);
    _occupied_ptr = reinterpret_cast<unsigned char*>(_occupied_ptr) + block_size;

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);

    auto memory_end = reinterpret_cast<unsigned char*>(_trusted_memory) + total_size;

    if (reinterpret_cast<unsigned char*>(_occupied_ptr) >= memory_end)
    {
        _occupied_ptr = nullptr;
        _occupied = false;
    }
    else
    {
        _occupied = *reinterpret_cast<bool*>(reinterpret_cast<unsigned char*>(_occupied_ptr) + sizeof(size_t));
    }

    return *this;
}

allocator_boundary_tags::boundary_iterator& allocator_boundary_tags::boundary_iterator::operator--() & noexcept
{
    if (_trusted_memory == nullptr)
    {
        return *this;
    }

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    auto first_block = current_ptr + sizeof(fit_mode) + sizeof(size_t) + sizeof(std::mutex) + sizeof(void*);

    if (_occupied_ptr == nullptr)
    {
        current_ptr += sizeof(fit_mode);
        size_t total_size = *reinterpret_cast<size_t*>(current_ptr);
        auto memory_end = reinterpret_cast<unsigned char*>(_trusted_memory) + total_size;

        auto prev_footer = memory_end - sizeof(size_t);
        size_t prev_size = *reinterpret_cast<size_t*>(prev_footer);
        _occupied_ptr = memory_end - prev_size;
        _occupied = *reinterpret_cast<bool*>(reinterpret_cast<unsigned char*>(_occupied_ptr) + sizeof(size_t));
        return *this;
    }

    if (reinterpret_cast<unsigned char*>(_occupied_ptr) <= first_block)
    {
        return *this;
    }

    auto prev_footer = reinterpret_cast<unsigned char*>(_occupied_ptr) - sizeof(size_t);
    size_t prev_size = *reinterpret_cast<size_t*>(prev_footer);
    _occupied_ptr = reinterpret_cast<unsigned char*>(_occupied_ptr) - prev_size;
    _occupied = *reinterpret_cast<bool*>(reinterpret_cast<unsigned char*>(_occupied_ptr) + sizeof(size_t));

    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int)
{
    auto temp = *this;
    --(*this);
    return temp;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept
{
    if (_occupied_ptr == nullptr)
    {
        return 0;
    }
    return *reinterpret_cast<size_t*>(_occupied_ptr);
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept
{
    return _occupied;
}

void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept
{
    return _occupied_ptr;
}

void* allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept
{
    return _occupied_ptr;
}
