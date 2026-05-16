#include "../include/allocator_red_black_tree.h"
#include <stdexcept>
#include <functional>
#include <new>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

using block_color = allocator_red_black_tree::block_color;
using block_data = allocator_red_black_tree::block_data;

inline block_color get_color(void* block) {
    if (block == nullptr) return block_color::BLACK;
    return reinterpret_cast<block_data*>(block)->color;
}

inline void set_color(void* block, block_color color) {
    if (block != nullptr) {
        reinterpret_cast<block_data*>(block)->color = color;
    }
}

inline bool is_occupied(void* block) {
    if (block == nullptr) return false;
    return reinterpret_cast<block_data*>(block)->occupied;
}

inline void set_occupied(void* block, bool occupied) {
    if (block != nullptr) {
        reinterpret_cast<block_data*>(block)->occupied = occupied;
    }
}

inline size_t* get_size_ptr(void* block) {
    return reinterpret_cast<size_t*>(reinterpret_cast<unsigned char*>(block) + allocator_red_black_tree::block_size_offset);
}

inline void** get_parent_ptr(void* block) {
    return reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(block) + allocator_red_black_tree::block_pointers_offset);
}

inline void** get_left_ptr(void* block) {
    return reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(block) + allocator_red_black_tree::block_pointers_offset + sizeof(void*));
}

inline void** get_right_ptr(void* block) {
    return reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(block) + allocator_red_black_tree::block_pointers_offset + 2 * sizeof(void*));
}

inline void** get_prev_physical_ptr(void* block) {
    if (is_occupied(block)) {
        return reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(block) + allocator_red_black_tree::block_pointers_offset);
    } else {
        return reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(block) + allocator_red_black_tree::block_pointers_offset + 3 * sizeof(void*));
    }
}

inline void** get_next_physical_ptr(void* block) {
    if (is_occupied(block)) {
        return reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(block) + allocator_red_black_tree::block_pointers_offset + sizeof(void*));
    } else {
        return reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(block) + allocator_red_black_tree::block_pointers_offset + 4 * sizeof(void*));
    }
}

inline void* get_parent(void* block) {
    return *get_parent_ptr(block);
}

inline void* get_left(void* block) {
    return *get_left_ptr(block);
}

inline void* get_right(void* block) {
    return *get_right_ptr(block);
}

inline void* get_prev_physical(void* block) {
    return *get_prev_physical_ptr(block);
}

inline void* get_next_physical(void* block) {
    return *get_next_physical_ptr(block);
}

inline size_t get_size(void* block) {
    return *get_size_ptr(block);
}

inline void set_parent(void* block, void* parent) {
    *get_parent_ptr(block) = parent;
}

inline void set_left(void* block, void* left) {
    *get_left_ptr(block) = left;
}

inline void set_right(void* block, void* right) {
    *get_right_ptr(block) = right;
}

inline void set_prev_physical(void* block, void* prev) {
    *get_prev_physical_ptr(block) = prev;
}

inline void set_next_physical(void* block, void* next) {
    *get_next_physical_ptr(block) = next;
}

inline void set_size(void* block, size_t size) {
    *get_size_ptr(block) = size;
}

inline size_t normalize_alignment(size_t alignment) {
    size_t result = std::max(alignment, alignof(void*));
    if ((result & (result - 1)) != 0) {
        throw std::bad_alloc();
    }
    return result;
}

inline unsigned char* align_up_ptr(unsigned char* ptr, size_t alignment) {
    auto value = reinterpret_cast<std::uintptr_t>(ptr);
    auto aligned = (value + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
    return reinterpret_cast<unsigned char*>(aligned);
}

inline size_t get_required_block_size(void* block, size_t user_size, size_t alignment) {
    auto block_begin = reinterpret_cast<unsigned char*>(block);
    auto user_begin = align_up_ptr(block_begin + allocator_red_black_tree::occupied_block_metadata_size + sizeof(void*),
                                   normalize_alignment(alignment));
    auto user_end = user_begin + user_size;
    return allocator_red_black_tree::align_up(static_cast<size_t>(user_end - block_begin),
                                              allocator_red_black_tree::block_alignment);
}

inline void* get_user_data_ptr(void* block, size_t alignment) {
    auto block_begin = reinterpret_cast<unsigned char*>(block);
    auto user_begin = align_up_ptr(block_begin + allocator_red_black_tree::occupied_block_metadata_size + sizeof(void*),
                                   normalize_alignment(alignment));
    *reinterpret_cast<void**>(user_begin - sizeof(void*)) = block;
    return user_begin;
}

inline void* get_block_from_user_ptr(void* user_ptr) {
    void* block = nullptr;
    std::memcpy(&block, reinterpret_cast<unsigned char*>(user_ptr) - sizeof(void*), sizeof(void*));
    return block;
}

// RB-tree operations

inline void** get_root_ptr(void* trusted_memory) {
    return reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(trusted_memory) +
        sizeof(std::pmr::memory_resource*) + sizeof(allocator_red_black_tree::fit_mode) +
        sizeof(size_t) + sizeof(std::mutex));
}

inline void* get_root(void* trusted_memory) {
    return *get_root_ptr(trusted_memory);
}

inline void set_root(void* trusted_memory, void* root) {
    *get_root_ptr(trusted_memory) = root;
}

void rotate_left(void* trusted_memory, void* x) {
    void* y = get_right(x);
    if (y == nullptr) return;

    set_right(x, get_left(y));
    if (get_left(y) != nullptr) {
        set_parent(get_left(y), x);
    }

    set_parent(y, get_parent(x));

    if (get_parent(x) == nullptr) {
        set_root(trusted_memory, y);
    } else if (x == get_left(get_parent(x))) {
        set_left(get_parent(x), y);
    } else {
        set_right(get_parent(x), y);
    }

    set_left(y, x);
    set_parent(x, y);
}

void rotate_right(void* trusted_memory, void* y) {
    void* x = get_left(y);
    if (x == nullptr) return;

    set_left(y, get_right(x));
    if (get_right(x) != nullptr) {
        set_parent(get_right(x), y);
    }

    set_parent(x, get_parent(y));

    if (get_parent(y) == nullptr) {
        set_root(trusted_memory, x);
    } else if (y == get_left(get_parent(y))) {
        set_left(get_parent(y), x);
    } else {
        set_right(get_parent(y), x);
    }

    set_right(x, y);
    set_parent(y, x);
}

void insert_fixup(void* trusted_memory, void* z) {
    while (z != get_root(trusted_memory) && get_color(get_parent(z)) == block_color::RED) {
        void* parent = get_parent(z);
        void* grandparent = get_parent(parent);

        if (grandparent == nullptr) break;

        if (parent == get_left(grandparent)) {
            void* uncle = get_right(grandparent);

            if (get_color(uncle) == block_color::RED) {
                set_color(parent, block_color::BLACK);
                set_color(uncle, block_color::BLACK);
                set_color(grandparent, block_color::RED);
                z = grandparent;
            } else {
                if (z == get_right(parent)) {
                    z = parent;
                    rotate_left(trusted_memory, z);
                    parent = get_parent(z);
                    grandparent = get_parent(parent);
                    if (grandparent == nullptr) break;
                }
                set_color(parent, block_color::BLACK);
                set_color(grandparent, block_color::RED);
                rotate_right(trusted_memory, grandparent);
            }
        } else {
            void* uncle = get_left(grandparent);

            if (get_color(uncle) == block_color::RED) {
                set_color(parent, block_color::BLACK);
                set_color(uncle, block_color::BLACK);
                set_color(grandparent, block_color::RED);
                z = grandparent;
            } else {
                if (z == get_left(parent)) {
                    z = parent;
                    rotate_right(trusted_memory, z);
                    parent = get_parent(z);
                    grandparent = get_parent(parent);
                    if (grandparent == nullptr) break;
                }
                set_color(parent, block_color::BLACK);
                set_color(grandparent, block_color::RED);
                rotate_left(trusted_memory, grandparent);
            }
        }
    }
    set_color(get_root(trusted_memory), block_color::BLACK);
}

void* tree_minimum(void* node) {
    while (get_left(node) != nullptr) {
        node = get_left(node);
    }
    return node;
}

void transplant(void* trusted_memory, void* u, void* v) {
    if (get_parent(u) == nullptr) {
        set_root(trusted_memory, v);
    } else if (u == get_left(get_parent(u))) {
        set_left(get_parent(u), v);
    } else {
        set_right(get_parent(u), v);
    }
    if (v != nullptr) {
        set_parent(v, get_parent(u));
    }
}

void delete_fixup(void* trusted_memory, void* x, void* x_parent) {
    while (x != get_root(trusted_memory) && get_color(x) == block_color::BLACK) {
        if (x_parent == nullptr) {
            break;
        }
        if (x == get_left(x_parent)) {
            void* w = get_right(x_parent);

            if (w == nullptr) {
                x = x_parent;
                x_parent = get_parent(x);
                continue;
            }

            if (get_color(w) == block_color::RED) {
                set_color(w, block_color::BLACK);
                set_color(x_parent, block_color::RED);
                rotate_left(trusted_memory, x_parent);
                w = get_right(x_parent);
            }

            if (get_color(get_left(w)) == block_color::BLACK &&
                get_color(get_right(w)) == block_color::BLACK) {
                set_color(w, block_color::RED);
                x = x_parent;
                x_parent = get_parent(x);
            } else {
                if (get_color(get_right(w)) == block_color::BLACK) {
                    set_color(get_left(w), block_color::BLACK);
                    set_color(w, block_color::RED);
                    rotate_right(trusted_memory, w);
                    w = get_right(x_parent);
                }
                set_color(w, get_color(x_parent));
                set_color(x_parent, block_color::BLACK);
                set_color(get_right(w), block_color::BLACK);
                rotate_left(trusted_memory, x_parent);
                x = get_root(trusted_memory);
                break;
            }
        } else {
            void* w = get_left(x_parent);

            if (w == nullptr) {
                x = x_parent;
                x_parent = get_parent(x);
                continue;
            }

            if (get_color(w) == block_color::RED) {
                set_color(w, block_color::BLACK);
                set_color(x_parent, block_color::RED);
                rotate_right(trusted_memory, x_parent);
                w = get_left(x_parent);
            }

            if (get_color(get_right(w)) == block_color::BLACK &&
                get_color(get_left(w)) == block_color::BLACK) {
                set_color(w, block_color::RED);
                x = x_parent;
                x_parent = get_parent(x);
            } else {
                if (get_color(get_left(w)) == block_color::BLACK) {
                    set_color(get_right(w), block_color::BLACK);
                    set_color(w, block_color::RED);
                    rotate_left(trusted_memory, w);
                    w = get_left(x_parent);
                }
                set_color(w, get_color(x_parent));
                set_color(x_parent, block_color::BLACK);
                set_color(get_left(w), block_color::BLACK);
                rotate_right(trusted_memory, x_parent);
                x = get_root(trusted_memory);
                break;
            }
        }
    }
    if (x != nullptr) {
        set_color(x, block_color::BLACK);
    }
}

void rb_delete(void* trusted_memory, void* z) {
    void* y = z;
    void* x;
    void* x_parent;
    block_color y_original_color = get_color(y);

    if (get_left(z) == nullptr) {
        x = get_right(z);
        x_parent = get_parent(z);
        transplant(trusted_memory, z, get_right(z));
    } else if (get_right(z) == nullptr) {
        x = get_left(z);
        x_parent = get_parent(z);
        transplant(trusted_memory, z, get_left(z));
    } else {
        y = tree_minimum(get_right(z));
        y_original_color = get_color(y);
        x = get_right(y);
        x_parent = y;

        if (get_parent(y) == z) {
            if (x != nullptr) {
                set_parent(x, y);
            }
        } else {
            x_parent = get_parent(y);
            transplant(trusted_memory, y, get_right(y));
            set_right(y, get_right(z));
            set_parent(get_right(y), y);
        }

        transplant(trusted_memory, z, y);
        set_left(y, get_left(z));
        set_parent(get_left(y), y);
        set_color(y, get_color(z));
    }

    if (y_original_color == block_color::BLACK) {
        delete_fixup(trusted_memory, x, x_parent);
    }
}

void rb_insert(void* trusted_memory, void* z) {
    void* y = nullptr;
    void* x = get_root(trusted_memory);

    while (x != nullptr) {
        y = x;
        if (get_size(z) < get_size(x)) {
            x = get_left(x);
        } else {
            x = get_right(x);
        }
    }

    set_parent(z, y);

    if (y == nullptr) {
        set_root(trusted_memory, z);
    } else if (get_size(z) < get_size(y)) {
        set_left(y, z);
    } else {
        set_right(y, z);
    }

    set_left(z, nullptr);
    set_right(z, nullptr);
    set_color(z, block_color::RED);

    insert_fixup(trusted_memory, z);
}

} // anonymous namespace

allocator_red_black_tree::~allocator_red_black_tree()
{
    if (_trusted_memory == nullptr)
    {
        return;
    }

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    auto parent_allocator = *reinterpret_cast<std::pmr::memory_resource**>(current_ptr);
    current_ptr += sizeof(std::pmr::memory_resource*);
    current_ptr += sizeof(fit_mode);

    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);
    current_ptr += sizeof(size_t);

    reinterpret_cast<std::mutex*>(current_ptr)->~mutex();

    parent_allocator->deallocate(_trusted_memory, total_size);
    _trusted_memory = nullptr;
}

allocator_red_black_tree::allocator_red_black_tree(
    allocator_red_black_tree &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_red_black_tree &allocator_red_black_tree::operator=(
    allocator_red_black_tree &&other) noexcept
{
    if (this != &other)
    {
        if (_trusted_memory != nullptr)
        {
            this->~allocator_red_black_tree();
        }
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    return *this;
}

allocator_red_black_tree::allocator_red_black_tree(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < free_block_metadata_size)
    {
        throw std::bad_alloc();
    }

    size_t total_size = allocator_metadata_size + space_size + 3 * free_block_metadata_size;

    if (parent_allocator == nullptr)
    {
        _trusted_memory = ::operator new(total_size);
        parent_allocator = std::pmr::new_delete_resource();
    }
    else
    {
        _trusted_memory = parent_allocator->allocate(total_size);
    }

    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    *reinterpret_cast<std::pmr::memory_resource**>(current_ptr) = parent_allocator;
    current_ptr += sizeof(std::pmr::memory_resource*);

    *reinterpret_cast<fit_mode*>(current_ptr) = allocate_fit_mode;
    current_ptr += sizeof(fit_mode);

    *reinterpret_cast<size_t*>(current_ptr) = total_size;
    current_ptr += sizeof(size_t);

    new(current_ptr) std::mutex();
    current_ptr += sizeof(std::mutex);

    void* first_block = reinterpret_cast<unsigned char*>(_trusted_memory) + allocator_metadata_size;
    *reinterpret_cast<void**>(current_ptr) = nullptr;
    current_ptr += sizeof(void*);

    // Initialize first free block
    auto first_block_data = reinterpret_cast<block_data*>(first_block);
    first_block_data->occupied = false;
    first_block_data->color = block_color::BLACK;

    set_parent(first_block, nullptr);
    set_left(first_block, nullptr);
    set_right(first_block, nullptr);
    set_prev_physical(first_block, nullptr);
    set_next_physical(first_block, nullptr);
    set_size(first_block, total_size - allocator_metadata_size);

    // Insert first block into the tree
    rb_insert(_trusted_memory, first_block);
}

allocator_red_black_tree::allocator_red_black_tree(const allocator_red_black_tree &other)
    : _trusted_memory(nullptr)
{
    if (other._trusted_memory == nullptr)
    {
        return;
    }

    auto current_ptr = reinterpret_cast<unsigned char*>(other._trusted_memory);

    auto parent_allocator = *reinterpret_cast<std::pmr::memory_resource**>(current_ptr);
    current_ptr += sizeof(std::pmr::memory_resource*);

    auto mode = *reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);

    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);

    _trusted_memory = parent_allocator->allocate(total_size);

    auto new_current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    *reinterpret_cast<std::pmr::memory_resource**>(new_current_ptr) = parent_allocator;
    new_current_ptr += sizeof(std::pmr::memory_resource*);

    *reinterpret_cast<fit_mode*>(new_current_ptr) = mode;
    new_current_ptr += sizeof(fit_mode);

    *reinterpret_cast<size_t*>(new_current_ptr) = total_size;
    new_current_ptr += sizeof(size_t);

    new(new_current_ptr) std::mutex();
    new_current_ptr += sizeof(std::mutex);

    *reinterpret_cast<void**>(new_current_ptr) = nullptr;

    auto old_block = reinterpret_cast<unsigned char*>(other._trusted_memory) + allocator_metadata_size;
    auto new_block = reinterpret_cast<unsigned char*>(_trusted_memory) + allocator_metadata_size;
    auto old_memory_end = reinterpret_cast<unsigned char*>(other._trusted_memory) + total_size;
    void* previous_block = nullptr;

    while (old_block < old_memory_end)
    {
        size_t block_size = get_size(old_block);
        std::memcpy(new_block, old_block, block_size);

        set_size(new_block, block_size);
        set_occupied(new_block, is_occupied(old_block));
        set_prev_physical(new_block, previous_block);

        auto next_block = new_block + block_size;
        set_next_physical(new_block, next_block < reinterpret_cast<unsigned char*>(_trusted_memory) + total_size
                                      ? static_cast<void*>(next_block)
                                      : nullptr);

        if (!is_occupied(new_block))
        {
            set_parent(new_block, nullptr);
            set_left(new_block, nullptr);
            set_right(new_block, nullptr);
            rb_insert(_trusted_memory, new_block);
        }

        previous_block = new_block;
        old_block += block_size;
        new_block += block_size;
    }
}

allocator_red_black_tree &allocator_red_black_tree::operator=(const allocator_red_black_tree &other)
{
    if (this != &other)
    {
        this->~allocator_red_black_tree();
        new(this) allocator_red_black_tree(other);
    }
    return *this;
}

bool allocator_red_black_tree::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    auto other_rb = dynamic_cast<const allocator_red_black_tree*>(&other);
    return other_rb != nullptr && other_rb->_trusted_memory == _trusted_memory;
}

[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(
    size_t size)
{
    return do_allocate_sm(size, alignof(std::max_align_t));
}

[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(
    size_t size,
    size_t alignment)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(std::pmr::memory_resource*);

    alignment = normalize_alignment(alignment);

    auto mode = *reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);

    if (size == 0) {
        throw std::bad_alloc();
    }

    void* root = get_root(_trusted_memory);
    void* best_block = nullptr;
    size_t best_size = 0;
    size_t best_required_size = 0;

    if (mode == fit_mode::first_fit) {
        for (auto it = begin(); it != end(); ++it) {
            void* node = *it;
            if (!it.occupied()) {
                size_t block_size = it.size();
                size_t required_size = get_required_block_size(node, size, alignment);
                if (block_size >= required_size) {
                    best_block = node;
                    best_size = block_size;
                    best_required_size = required_size;
                    break;
                }
            }
        }
    } else if (mode == fit_mode::the_best_fit) {
        std::function<void(void*)> find_best = [&](void* node) {
            if (node == nullptr) return;

            size_t block_size = get_size(node);
            size_t required_size = get_required_block_size(node, size, alignment);

            if (block_size >= required_size) {
                if (best_block == nullptr || block_size < best_size) {
                    best_block = node;
                    best_size = block_size;
                    best_required_size = required_size;
                }
                find_best(get_left(node));
            } else {
                find_best(get_right(node));
            }
        };
        find_best(root);
    } else if (mode == fit_mode::the_worst_fit) {
        std::function<void(void*)> find_worst = [&](void* node) {
            if (node == nullptr) return;

            size_t block_size = get_size(node);
            size_t required_size = get_required_block_size(node, size, alignment);

            if (block_size >= required_size) {
                if (best_block == nullptr || block_size > best_size) {
                    best_block = node;
                    best_size = block_size;
                    best_required_size = required_size;
                }
            }

            find_worst(get_left(node));
            find_worst(get_right(node));
        };
        find_worst(root);
    }

    if (best_block == nullptr) {
        throw std::bad_alloc();
    }

    void* prev_phys = get_prev_physical(best_block);
    void* next_phys = get_next_physical(best_block);

    rb_delete(_trusted_memory, best_block);

    size_t remaining_size = best_size - best_required_size;

    if (remaining_size >= free_block_metadata_size) {
        void* new_free_block = reinterpret_cast<unsigned char*>(best_block) + best_required_size;

        auto new_block_data = reinterpret_cast<block_data*>(new_free_block);
        new_block_data->occupied = false;
        new_block_data->color = block_color::BLACK;

        set_parent(new_free_block, nullptr);
        set_left(new_free_block, nullptr);
        set_right(new_free_block, nullptr);

        set_prev_physical(new_free_block, best_block);
        set_next_physical(new_free_block, next_phys);

        if (next_phys != nullptr) {
            set_prev_physical(next_phys, new_free_block);
        }

        set_size(new_free_block, remaining_size);

        rb_insert(_trusted_memory, new_free_block);

        set_occupied(best_block, true);
        set_size(best_block, best_required_size);

        set_prev_physical(best_block, prev_phys);
        set_next_physical(best_block, new_free_block);
    } else {
        set_occupied(best_block, true);
        set_size(best_block, best_size);

        set_prev_physical(best_block, prev_phys);
        set_next_physical(best_block, next_phys);
    }

    return get_user_data_ptr(best_block, alignment);
}


void allocator_red_black_tree::do_deallocate_sm(
    void *at)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);

    auto parent_allocator = *reinterpret_cast<std::pmr::memory_resource**>(current_ptr);
    current_ptr += sizeof(std::pmr::memory_resource*);
    current_ptr += sizeof(fit_mode);

    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);
    current_ptr += sizeof(size_t);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);
    auto memory_start = reinterpret_cast<unsigned char*>(_trusted_memory) + allocator_metadata_size;
    auto memory_end = reinterpret_cast<unsigned char*>(_trusted_memory) + total_size;

    if (reinterpret_cast<unsigned char*>(at) < memory_start ||
        reinterpret_cast<unsigned char*>(at) >= memory_end ||
        reinterpret_cast<unsigned char*>(at) - reinterpret_cast<unsigned char*>(_trusted_memory) < sizeof(void*)) {
        throw std::logic_error("Block does not belong to this allocator");
    }

    void* block = get_block_from_user_ptr(at);

    if (reinterpret_cast<unsigned char*>(block) < memory_start ||
        reinterpret_cast<unsigned char*>(block) >= memory_end) {
        throw std::logic_error("Block does not belong to this allocator");
    }

    bool block_start_exists = false;
    for (auto it = begin(); it != end(); ++it) {
        if (*it == block) {
            block_start_exists = true;
            break;
        }
    }

    if (!block_start_exists) {
        throw std::logic_error("Block does not belong to this allocator");
    }

    if (!is_occupied(block)) {
        throw std::logic_error("Double free detected");
    }

    size_t block_size = get_size(block);

    void* prev_phys = get_prev_physical(block);
    void* next_phys = get_next_physical(block);

    set_occupied(block, false);

    if (prev_phys != nullptr && !is_occupied(prev_phys)) {
        rb_delete(_trusted_memory, prev_phys);

        size_t prev_size = get_size(prev_phys);
        block_size += prev_size;

        void* prev_prev = get_prev_physical(prev_phys);

        block = prev_phys;
        prev_phys = prev_prev;
    }

    if (next_phys != nullptr && !is_occupied(next_phys)) {
        rb_delete(_trusted_memory, next_phys);

        size_t next_size = get_size(next_phys);
        block_size += next_size;

        void* next_next = get_next_physical(next_phys);
        next_phys = next_next;
    }

    set_size(block, block_size);

    set_parent(block, nullptr);
    set_left(block, nullptr);
    set_right(block, nullptr);

    set_prev_physical(block, prev_phys);
    set_next_physical(block, next_phys);

    if (prev_phys != nullptr) {
        set_next_physical(prev_phys, block);
    }
    if (next_phys != nullptr) {
        set_prev_physical(next_phys, block);
    }

    rb_insert(_trusted_memory, block);
}

void allocator_red_black_tree::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(std::pmr::memory_resource*);

    auto mutex_ptr = reinterpret_cast<std::mutex*>(reinterpret_cast<unsigned char*>(_trusted_memory) + sizeof(std::pmr::memory_resource*) + sizeof(fit_mode) + sizeof(size_t));
    std::lock_guard<std::mutex> lock(*mutex_ptr);

    *reinterpret_cast<fit_mode*>(current_ptr) = mode;
}


std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const
{
    auto mutex_ptr = reinterpret_cast<std::mutex*>(reinterpret_cast<unsigned char*>(_trusted_memory) +
        sizeof(std::pmr::memory_resource*) + sizeof(fit_mode) + sizeof(size_t));
    std::lock_guard<std::mutex> lock(*mutex_ptr);

    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const
{
    std::vector<block_info> result;

    for (auto it = begin(); it != end(); ++it) {
        void* block = *it;
        block_info info;
        info.block_size = it.size();
        info.is_block_occupied = it.occupied();
        result.push_back(info);
    }

    return result;
}


allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept
{
    return rb_iterator(reinterpret_cast<unsigned char*>(_trusted_memory) + allocator_metadata_size, _trusted_memory);
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept
{
    return rb_iterator();
}


bool allocator_red_black_tree::rb_iterator::operator==(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return _block_ptr == other._block_ptr;
}

bool allocator_red_black_tree::rb_iterator::operator!=(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return _block_ptr != other._block_ptr;
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept
{
    if (_block_ptr == nullptr) {
        return *this;
    }

    bool is_occ = is_occupied(_block_ptr);
    size_t block_size = get_size(_block_ptr);

    void* next = reinterpret_cast<unsigned char*>(_block_ptr) + block_size;

    auto trusted_start = reinterpret_cast<unsigned char*>(_trusted);
    auto metadata_size = sizeof(std::pmr::memory_resource*) + sizeof(allocator_with_fit_mode::fit_mode) +
                         sizeof(size_t) + sizeof(std::mutex) + sizeof(void*);

    auto total_size_ptr = reinterpret_cast<size_t*>(trusted_start + sizeof(std::pmr::memory_resource*) +
                                                     sizeof(allocator_with_fit_mode::fit_mode));
    size_t total_size = *total_size_ptr;

    auto memory_end = trusted_start + total_size;

    if (reinterpret_cast<unsigned char*>(next) >= memory_end) {
        _block_ptr = nullptr;
    } else {
        _block_ptr = next;
    }

    return *this;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int)
{
    rb_iterator tmp = *this;
    ++(*this);
    return tmp;
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept
{
    if (_block_ptr == nullptr) {
        return 0;
    }
    return get_size(_block_ptr);
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept
{
    return _block_ptr;
}

allocator_red_black_tree::rb_iterator::rb_iterator()
    : _block_ptr(nullptr), _trusted(nullptr)
{
}

allocator_red_black_tree::rb_iterator::rb_iterator(void *block_ptr, void *trusted_memory)
    : _block_ptr(block_ptr), _trusted(trusted_memory)
{
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept
{
    if (_block_ptr == nullptr) {
        return false;
    }
    return is_occupied(_block_ptr);
}
