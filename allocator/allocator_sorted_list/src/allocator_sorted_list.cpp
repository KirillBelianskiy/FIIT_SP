#include "../include/allocator_sorted_list.h"
#include <stdexcept>

allocator_sorted_list::allocator_sorted_list(
    size_t space_size,
    allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < block_metadata_size)
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
    *reinterpret_cast<void**>(first_block_ptr) = nullptr;
    *reinterpret_cast<size_t*>(first_block_ptr + sizeof(void*)) = space_size;
    *reinterpret_cast<bool*>(first_block_ptr + sizeof(void*) + sizeof(size_t)) = false;
}

allocator_sorted_list::~allocator_sorted_list()
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

allocator_sorted_list::allocator_sorted_list(
    allocator_sorted_list&& other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_sorted_list& allocator_sorted_list::operator=(
    allocator_sorted_list&& other) noexcept
{
    if (this != &other)
    {
        if (_trusted_memory != nullptr)
        {
            this->~allocator_sorted_list();
        }
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    return *this;
}

allocator_sorted_list::allocator_sorted_list(const allocator_sorted_list& other)
    : _trusted_memory(nullptr)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(other._trusted_memory);

    auto mode = *reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);

    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);

    size_t space_size = total_size - allocator_metadata_size;

    *this = allocator_sorted_list(space_size, mode);
}

allocator_sorted_list& allocator_sorted_list::operator=(const allocator_sorted_list& other)
{
    if (this != &other)
    {
        this->~allocator_sorted_list();
        new(this) allocator_sorted_list(other);
    }
    return *this;
}

bool allocator_sorted_list::do_is_equal(const std::pmr::memory_resource& other) const noexcept
{
    auto other_sorted = dynamic_cast<const allocator_sorted_list*>(&other);
    return other_sorted != nullptr && other_sorted->_trusted_memory == _trusted_memory;
}

[[nodiscard]] void* allocator_sorted_list::do_allocate_sm(size_t size)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    auto mode = *reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);
    current_ptr += sizeof(std::mutex);

    auto first_free_ptr = reinterpret_cast<void**>(current_ptr);

    if (size == 0)
    {
        throw std::bad_alloc();
    }

    void** best_prev = nullptr;
    void* best_block = nullptr;
    size_t best_size = 0;

    void** prev_ptr = first_free_ptr;
    void* current_block = *first_free_ptr;

    while (current_block != nullptr)
    {
        auto block_ptr = reinterpret_cast<unsigned char*>(current_block);
        bool is_occupied = *reinterpret_cast<bool*>(block_ptr + sizeof(void*) + sizeof(size_t));

        if (is_occupied)
        {
            prev_ptr = reinterpret_cast<void**>(current_block);
            current_block = *reinterpret_cast<void**>(current_block);
            continue;
        }

        size_t block_size = *reinterpret_cast<size_t*>(block_ptr + sizeof(void*));

        if (block_size >= size + block_metadata_size)
        {
            if (mode == fit_mode::first_fit)
            {
                best_prev = prev_ptr;
                best_block = current_block;
                best_size = block_size;
                break;
            }
            if (mode == fit_mode::the_best_fit)
            {
                if (best_block == nullptr || block_size < best_size)
                {
                    best_prev = prev_ptr;
                    best_block = current_block;
                    best_size = block_size;
                }
            }
            else if (mode == fit_mode::the_worst_fit)
            {
                if (best_block == nullptr || block_size > best_size)
                {
                    best_prev = prev_ptr;
                    best_block = current_block;
                    best_size = block_size;
                }
            }
        }

        prev_ptr = reinterpret_cast<void**>(current_block);
        current_block = *reinterpret_cast<void**>(current_block);
    }

    if (best_block == nullptr)
    {
        throw std::bad_alloc();
    }

    size_t remaining_size = best_size - size - block_metadata_size;

    if (remaining_size >= block_metadata_size)
    {
        void* new_free_block = reinterpret_cast<unsigned char*>(best_block) + block_metadata_size + size;
        void* next_block = *reinterpret_cast<void**>(best_block);

        auto new_block_ptr = reinterpret_cast<unsigned char*>(new_free_block);
        *reinterpret_cast<void**>(new_block_ptr) = next_block;
        *reinterpret_cast<size_t*>(new_block_ptr + sizeof(void*)) = remaining_size;
        *reinterpret_cast<bool*>(new_block_ptr + sizeof(void*) + sizeof(size_t)) = false;

        *best_prev = new_free_block;

        auto best_block_ptr = reinterpret_cast<unsigned char*>(best_block);
        *reinterpret_cast<size_t*>(best_block_ptr + sizeof(void*)) = size;
    }
    else
    {
        *best_prev = *reinterpret_cast<void**>(best_block);
    }

    auto best_block_ptr = reinterpret_cast<unsigned char*>(best_block);
    *reinterpret_cast<bool*>(best_block_ptr + sizeof(void*) + sizeof(size_t)) = true;

    return reinterpret_cast<unsigned char*>(best_block) + block_metadata_size;
}

void allocator_sorted_list::do_deallocate_sm(void* at)
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

    auto memory_start = reinterpret_cast<unsigned char*>(_trusted_memory);
    auto memory_end = memory_start + total_size;
    auto block_ptr = reinterpret_cast<unsigned char*>(at);

    if (block_ptr < memory_start || block_ptr >= memory_end)
    {
        throw std::logic_error("Block does not belong to this allocator");
    }

    void* block = block_ptr - block_metadata_size;

    auto block_data_ptr = reinterpret_cast<unsigned char*>(block);
    bool is_occupied = *reinterpret_cast<bool*>(block_data_ptr + sizeof(void*) + sizeof(size_t));

    if (!is_occupied)
    {
        throw std::logic_error("Double free or invalid pointer");
    }

    *reinterpret_cast<bool*>(block_data_ptr + sizeof(void*) + sizeof(size_t)) = false;

    auto first_free_ptr = reinterpret_cast<void**>(current_ptr);

    void** prev_ptr = first_free_ptr;
    void* current_block = *first_free_ptr;

    while (current_block != nullptr && current_block < block)
    {
        prev_ptr = reinterpret_cast<void**>(current_block);
        current_block = *reinterpret_cast<void**>(current_block);
    }

    *reinterpret_cast<void**>(block) = current_block;
    *prev_ptr = block;

    size_t block_size = *reinterpret_cast<size_t*>(reinterpret_cast<unsigned char*>(block) + sizeof(void*));
    void* next_block = *reinterpret_cast<void**>(block);

    if (next_block != nullptr)
    {
        auto next_block_ptr = reinterpret_cast<unsigned char*>(next_block);
        bool next_is_occupied = *reinterpret_cast<bool*>(next_block_ptr + sizeof(void*) + sizeof(size_t));

        auto expected_next = reinterpret_cast<unsigned char*>(block) + block_metadata_size + block_size;
        if (expected_next == reinterpret_cast<unsigned char*>(next_block) && !next_is_occupied)
        {
            size_t next_size = *reinterpret_cast<size_t*>(next_block_ptr + sizeof(void*));
            void* next_next = *reinterpret_cast<void**>(next_block);

            *reinterpret_cast<void**>(block) = next_next;
            *reinterpret_cast<size_t*>(reinterpret_cast<unsigned char*>(block) + sizeof(void*)) = block_size +
                block_metadata_size + next_size;
        }
    }

    if (prev_ptr != first_free_ptr)
    {
        void* prev_block = prev_ptr;
        auto prev_block_ptr = reinterpret_cast<unsigned char*>(prev_block);
        bool prev_is_occupied = *reinterpret_cast<bool*>(prev_block_ptr + sizeof(void*) + sizeof(size_t));
        size_t prev_size = *reinterpret_cast<size_t*>(prev_block_ptr + sizeof(void*));

        auto expected_current = reinterpret_cast<unsigned char*>(prev_block) + block_metadata_size + prev_size;
        if (expected_current == reinterpret_cast<unsigned char*>(block) && !prev_is_occupied)
        {
            block_size = *reinterpret_cast<size_t*>(reinterpret_cast<unsigned char*>(block) + sizeof(void*));
            void* block_next = *reinterpret_cast<void**>(block);

            *reinterpret_cast<void**>(prev_block) = block_next;
            *reinterpret_cast<size_t*>(prev_block_ptr + sizeof(void*)) = prev_size +
                block_metadata_size + block_size;
        }
    }
}

inline void allocator_sorted_list::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    auto mode_ptr = reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);

    *mode_ptr = mode;
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);

    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info_inner() const
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

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator()
    : _free_ptr(nullptr)
{
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(void* trusted)
    : _free_ptr(trusted)
{
}

bool allocator_sorted_list::sorted_free_iterator::operator==(
    const allocator_sorted_list::sorted_free_iterator& other) const noexcept
{
    return _free_ptr == other._free_ptr;
}

bool allocator_sorted_list::sorted_free_iterator::operator!=(
    const allocator_sorted_list::sorted_free_iterator& other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_free_iterator& allocator_sorted_list::sorted_free_iterator::operator++() & noexcept
{
    if (_free_ptr != nullptr)
    {
        _free_ptr = *reinterpret_cast<void**>(_free_ptr);
    }
    return *this;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::sorted_free_iterator::operator++(int)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_sorted_list::sorted_free_iterator::size() const noexcept
{
    if (_free_ptr == nullptr)
    {
        return 0;
    }
    return *reinterpret_cast<size_t*>(reinterpret_cast<unsigned char*>(_free_ptr) + sizeof(void*));
}

void* allocator_sorted_list::sorted_free_iterator::operator*() const noexcept
{
    return _free_ptr;
}

allocator_sorted_list::sorted_iterator::sorted_iterator()
    : _free_ptr(nullptr), _current_ptr(nullptr), _trusted_memory(nullptr)
{
}

allocator_sorted_list::sorted_iterator::sorted_iterator(void* trusted)
    : _trusted_memory(trusted)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(trusted);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);
    current_ptr += sizeof(std::mutex);

    _free_ptr = *reinterpret_cast<void**>(current_ptr);
    _current_ptr = current_ptr + sizeof(void*);
}

bool allocator_sorted_list::sorted_iterator::operator==(
    const allocator_sorted_list::sorted_iterator& other) const noexcept
{
    return _current_ptr == other._current_ptr;
}

bool allocator_sorted_list::sorted_iterator::operator!=(
    const allocator_sorted_list::sorted_iterator& other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_iterator& allocator_sorted_list::sorted_iterator::operator++() & noexcept
{
    if (_current_ptr == nullptr)
    {
        return *this;
    }

    size_t block_size = *reinterpret_cast<size_t*>(reinterpret_cast<unsigned char*>(_current_ptr) + sizeof(void*));
    _current_ptr = reinterpret_cast<unsigned char*>(_current_ptr) + block_metadata_size + block_size;

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);

    auto memory_end = reinterpret_cast<unsigned char*>(_trusted_memory) + total_size;

    if (reinterpret_cast<unsigned char*>(_current_ptr) >= memory_end)
    {
        _current_ptr = nullptr;
    }

    return *this;
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::sorted_iterator::operator++(int)
{
    auto temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_sorted_list::sorted_iterator::size() const noexcept
{
    if (_current_ptr == nullptr)
    {
        return 0;
    }
    return *reinterpret_cast<size_t*>(reinterpret_cast<unsigned char*>(_current_ptr) + sizeof(void*));
}

void* allocator_sorted_list::sorted_iterator::operator*() const noexcept
{
    return _current_ptr;
}

bool allocator_sorted_list::sorted_iterator::occupied() const noexcept
{
    if (_current_ptr == nullptr)
    {
        return false;
    }
    auto block_ptr = reinterpret_cast<unsigned char*>(_current_ptr);
    return *reinterpret_cast<bool*>(block_ptr + sizeof(void*) + sizeof(size_t));
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_begin() const noexcept
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);
    current_ptr += sizeof(std::mutex);

    return sorted_free_iterator(*reinterpret_cast<void**>(current_ptr));
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_end() const noexcept
{
    return sorted_free_iterator(nullptr);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::begin() const noexcept
{
    return sorted_iterator(_trusted_memory);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::end() const noexcept
{
    return sorted_iterator();
}
