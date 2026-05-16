#ifndef MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_RED_BLACK_TREE_H
#define MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_RED_BLACK_TREE_H

#include <pp_allocator.h>
#include <allocator_test_utils.h>
#include <allocator_with_fit_mode.h>
#include <mutex>

class allocator_red_black_tree final:
    public smart_mem_resource,
    public allocator_test_utils,
    public allocator_with_fit_mode
{

public:

    static constexpr size_t block_alignment = alignof(std::max_align_t);

    static constexpr size_t align_up(size_t size, size_t alignment) noexcept
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    enum class block_color : unsigned char
    { RED, BLACK };

    struct block_data
    {
        bool occupied : 4;
        block_color color : 4;
    };

    static constexpr size_t block_size_offset = (sizeof(block_data) + alignof(size_t) - 1) & ~(alignof(size_t) - 1);
    static constexpr size_t block_pointers_offset = (block_size_offset + sizeof(size_t) + alignof(void*) - 1) & ~(alignof(void*) - 1);
    static constexpr size_t allocator_metadata_size = (sizeof(allocator_dbg_helper*) + sizeof(fit_mode) + sizeof(size_t) + sizeof(std::mutex) + sizeof(void*) + block_alignment - 1) & ~(block_alignment - 1);
    static constexpr size_t occupied_block_metadata_size = (block_pointers_offset + 2 * sizeof(void*) + block_alignment - 1) & ~(block_alignment - 1);
    static constexpr size_t free_block_metadata_size = (block_pointers_offset + 5 * sizeof(void*) + block_alignment - 1) & ~(block_alignment - 1);

private:

    void *_trusted_memory;

public:

    ~allocator_red_black_tree() override;

    allocator_red_black_tree(
        allocator_red_black_tree const &other);

    allocator_red_black_tree &operator=(
        allocator_red_black_tree const &other);

    allocator_red_black_tree(
        allocator_red_black_tree &&other) noexcept;

    allocator_red_black_tree &operator=(
        allocator_red_black_tree &&other) noexcept;

public:

    explicit allocator_red_black_tree(
            size_t space_size,
            std::pmr::memory_resource *parent_allocator = nullptr,
            allocator_with_fit_mode::fit_mode allocate_fit_mode = allocator_with_fit_mode::fit_mode::first_fit);

    std::vector<allocator_test_utils::block_info> get_blocks_info() const override;

    void set_fit_mode(allocator_with_fit_mode::fit_mode mode) override;

private:

    [[nodiscard]] void *do_allocate_sm(
        size_t size) override;

    [[nodiscard]] void *do_allocate_sm(
        size_t size,
        size_t alignment) override;

    void do_deallocate_sm(
        void *at) override;

    bool do_is_equal(const std::pmr::memory_resource&) const noexcept override;

    std::vector<allocator_test_utils::block_info> get_blocks_info_inner() const override;

    class rb_iterator
    {
        void* _block_ptr;
        void* _trusted;

    public:

        using iterator_category = std::forward_iterator_tag;
        using value_type = void*;
        using reference = void*&;
        using pointer = void**;
        using difference_type = ptrdiff_t;

        bool operator==(const rb_iterator&) const noexcept;

        bool operator!=(const rb_iterator&) const noexcept;

        rb_iterator& operator++() & noexcept;

        rb_iterator operator++(int);

        size_t size() const noexcept;

        void* operator*() const noexcept;

        bool occupied()const noexcept;

        rb_iterator();

        rb_iterator(void* block_ptr, void* trusted_memory);
    };

    friend class rb_iterator;

    rb_iterator begin() const noexcept;
    rb_iterator end() const noexcept;

};

#endif //MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_RED_BLACK_TREE_H
