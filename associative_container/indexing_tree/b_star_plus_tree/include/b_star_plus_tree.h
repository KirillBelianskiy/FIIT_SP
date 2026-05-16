#ifndef SYS_PROG_BS_PLUS_TREE_H
#define SYS_PROG_BS_PLUS_TREE_H

#include <iterator>
#include <utility>
#include <boost/container/static_vector.hpp>
#include <stack>
#include <vector>
#include <pp_allocator.h>
#include <associative_container.h>
#include <initializer_list>

template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5>
class BSP_tree final : private compare // EBCO
{
public:
    using tree_data_type = std::pair<tkey, tvalue>;
    using tree_data_type_const = std::pair<const tkey, tvalue>;
    using value_type = tree_data_type_const;

private:
    static constexpr const size_t minimum_keys_in_node = t - 1;
    static constexpr const size_t maximum_keys_in_node = 2 * t - 1;

    // region comparators declaration

    inline bool compare_keys(const tkey& lhs, const tkey& rhs) const;
    inline bool compare_pairs(const tree_data_type& lhs, const tree_data_type& rhs) const;

    // endregion comparators declaration


    struct bsptree_node
    {
        boost::container::static_vector<tree_data_type, maximum_keys_in_node + 1> _keys;
        boost::container::static_vector<bsptree_node*, maximum_keys_in_node + 2> _pointers;
        bsptree_node() noexcept;
    };

    pp_allocator<value_type> _allocator;
    bsptree_node* _root;
    size_t _size;

    void split_child(bsptree_node* node, std::stack<bsptree_node*>& path);

    pp_allocator<value_type> get_allocator() const noexcept;

public:
    // region constructors declaration

    explicit BSP_tree(const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    explicit BSP_tree(pp_allocator<value_type> alloc, const compare& comp = compare());

    template <input_iterator_for_pair<tkey, tvalue> iterator>
    explicit BSP_tree(iterator begin, iterator end, const compare& cmp = compare(),
                    pp_allocator<value_type> = pp_allocator<value_type>());

    BSP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(),
           pp_allocator<value_type> = pp_allocator<value_type>());

    // endregion constructors declaration

    // region five declaration

    BSP_tree(const BSP_tree& other);

    BSP_tree(BSP_tree&& other) noexcept;

    BSP_tree& operator=(const BSP_tree& other);

    BSP_tree& operator=(BSP_tree&& other) noexcept;

    ~BSP_tree() noexcept;

    // endregion five declaration

    // region iterators declaration

    class bsptree_iterator;
    class bsptree_reverse_iterator;
    class bsptree_const_iterator;
    class bsptree_const_reverse_iterator;

    class bsptree_iterator final
    {
        std::stack<std::pair<bsptree_node**, size_t>> _path;
        size_t _index;
        BSP_tree* _owner;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bsptree_iterator;

        friend class BSP_tree;
        friend class bsptree_reverse_iterator;
        friend class bsptree_const_iterator;
        friend class bsptree_const_reverse_iterator;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit bsptree_iterator(
            const std::stack<std::pair<bsptree_node**, size_t>>& path = std::stack<std::pair<bsptree_node**, size_t>>(),
            size_t index = 0, BSP_tree* owner = nullptr);
    };

    class bsptree_const_iterator final
    {
        std::stack<std::pair<bsptree_node* const*, size_t>> _path;
        size_t _index;
        const BSP_tree* _owner;

    public:
        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bsptree_const_iterator;

        friend class BSP_tree;
        friend class bsptree_reverse_iterator;
        friend class bsptree_iterator;
        friend class bsptree_const_reverse_iterator;

        bsptree_const_iterator(const bsptree_iterator& it) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit bsptree_const_iterator(
            const std::stack<std::pair<bsptree_node* const*, size_t>>& path = std::stack<std::pair<
                bsptree_node* const*, size_t>>(), size_t index = 0, const BSP_tree* owner = nullptr);
    };

    class bsptree_reverse_iterator final
    {
        std::stack<std::pair<bsptree_node**, size_t>> _path;
        size_t _index;
        BSP_tree* _owner;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bsptree_reverse_iterator;

        friend class BSP_tree;
        friend class bsptree_iterator;
        friend class bsptree_const_iterator;
        friend class bsptree_const_reverse_iterator;

        bsptree_reverse_iterator(const bsptree_iterator& it) noexcept;
        operator bsptree_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit bsptree_reverse_iterator(
            const std::stack<std::pair<bsptree_node**, size_t>>& path = std::stack<std::pair<bsptree_node**, size_t>>(),
            size_t index = 0, BSP_tree* owner = nullptr);
    };

    class bsptree_const_reverse_iterator final
    {
        std::stack<std::pair<bsptree_node* const*, size_t>> _path;
        size_t _index;
        const BSP_tree* _owner;

    public:
        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bsptree_const_reverse_iterator;

        friend class BSP_tree;
        friend class bsptree_reverse_iterator;
        friend class bsptree_const_iterator;
        friend class bsptree_iterator;

        bsptree_const_reverse_iterator(const bsptree_reverse_iterator& it) noexcept;
        operator bsptree_const_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit bsptree_const_reverse_iterator(
            const std::stack<std::pair<bsptree_node* const*, size_t>>& path = std::stack<std::pair<
                bsptree_node* const*, size_t>>(), size_t index = 0, const BSP_tree* owner = nullptr);
    };

    friend class bsptree_iterator;
    friend class bsptree_const_iterator;
    friend class bsptree_reverse_iterator;
    friend class bsptree_const_reverse_iterator;

    // endregion iterators declaration

    // region element access declaration

    /*
     * Returns a reference to the mapped value of the element with specified key. If no such element exists, an exception of type std::out_of_range is thrown.
     */
    tvalue& at(const tkey&);
    const tvalue& at(const tkey&) const;

    /*
     * If key not exists, makes default initialization of value
     */
    tvalue& operator[](const tkey& key);
    tvalue& operator[](tkey&& key);

    // endregion element access declaration
    // region iterator begins declaration

    bsptree_iterator begin();
    bsptree_iterator end();

    bsptree_const_iterator begin() const;
    bsptree_const_iterator end() const;

    bsptree_const_iterator cbegin() const;
    bsptree_const_iterator cend() const;

    bsptree_reverse_iterator rbegin();
    bsptree_reverse_iterator rend();

    bsptree_const_reverse_iterator rbegin() const;
    bsptree_const_reverse_iterator rend() const;

    bsptree_const_reverse_iterator crbegin() const;
    bsptree_const_reverse_iterator crend() const;

    // endregion iterator begins declaration

    // region lookup declaration

    size_t size() const noexcept;
    bool empty() const noexcept;

    /*
     * Returns end() if not exist
     */

    bsptree_iterator find(const tkey& key);
    bsptree_const_iterator find(const tkey& key) const;

    bsptree_iterator lower_bound(const tkey& key);
    bsptree_const_iterator lower_bound(const tkey& key) const;

    bsptree_iterator upper_bound(const tkey& key);
    bsptree_const_iterator upper_bound(const tkey& key) const;

    bool contains(const tkey& key) const;

    // endregion lookup declaration

    // region modifiers declaration

    void clear() noexcept;

    /*
     * Does nothing if key exists, delegates to emplace.
     * Second return value is true, when inserted
     */
    std::pair<bsptree_iterator, bool> insert(const tree_data_type& data);
    std::pair<bsptree_iterator, bool> insert(tree_data_type&& data);

    template <typename... Args>
    std::pair<bsptree_iterator, bool> emplace(Args&&... args);

    /*
     * Updates value if key exists, delegates to emplace.
     */
    bsptree_iterator insert_or_assign(const tree_data_type& data);
    bsptree_iterator insert_or_assign(tree_data_type&& data);

    template <typename... Args>
    bsptree_iterator emplace_or_assign(Args&&... args);

    /*
     * Return iterator to node next ro removed or end() if key not exists
     */
    bsptree_iterator erase(bsptree_iterator pos);
    bsptree_iterator erase(bsptree_const_iterator pos);

    bsptree_iterator erase(bsptree_iterator beg, bsptree_iterator en);
    bsptree_iterator erase(bsptree_const_iterator beg, bsptree_const_iterator en);


    bsptree_iterator erase(const tkey& key);

    // endregion modifiers declaration
};

template <std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type>
                                        compare = std::less<typename std::iterator_traits<
                                            iterator>::value_type::first_type>,
                                        std::size_t t = 5, typename U>
BSP_tree(iterator begin, iterator end, const compare& cmp = compare(),
       pp_allocator<U> = pp_allocator<U>()) -> BSP_tree<
    typename std::iterator_traits<iterator>::value_type::first_type, typename std::iterator_traits<
        iterator>::value_type::second_type, compare, t>;

template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5, typename U>
BSP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(),
       pp_allocator<U> = pp_allocator<U>()) -> BSP_tree<tkey, tvalue, compare, t>;

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::compare_pairs(const BSP_tree::tree_data_type& lhs,
                                                     const BSP_tree::tree_data_type& rhs) const
{
    return compare_keys(lhs.first, rhs.first);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::compare_keys(const tkey& lhs, const tkey& rhs) const
{
    return compare::operator()(lhs, rhs);
}


template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_node::bsptree_node() noexcept
{
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
pp_allocator<typename BSP_tree<tkey, tvalue, compare, t>::value_type> BSP_tree<
    tkey, tvalue, compare, t>::get_allocator() const noexcept
{
    return _allocator;
}

// region constructors implementation

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::BSP_tree(
    const compare& cmp,
    pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(alloc), _root(nullptr), _size(0)
{
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::BSP_tree(
    pp_allocator<value_type> alloc,\
    const compare& comp)
    : compare(comp), _allocator(alloc), _root(nullptr), _size(0)
{
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template <input_iterator_for_pair<tkey, tvalue> iterator>
BSP_tree<tkey, tvalue, compare, t>::BSP_tree(
    iterator begin,
    iterator end,
    const compare& cmp,
    pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(alloc), _root(nullptr), _size(0)
{
    for (auto it = begin; it != end; ++it)
    {
        insert(*it);
    }
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::BSP_tree(
    std::initializer_list<std::pair<tkey, tvalue>> data,
    const compare& cmp,
    pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(alloc), _root(nullptr), _size(0)
{
    for (const auto& item : data)
    {
        insert(item);
    }
}

// endregion constructors implementation

// region five implementation

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::~BSP_tree() noexcept
{
    clear();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::BSP_tree(const BSP_tree& other)
    : compare(static_cast<const compare&>(other)), _allocator(other._allocator), _root(nullptr), _size(0)
{
    for (const auto& item : other)
    {
        insert(item);
    }
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>& BSP_tree<tkey, tvalue, compare, t>::operator=(const BSP_tree& other)
{
    if (this != &other)
    {
        clear();
        compare::operator=(static_cast<const compare&>(other));
        _allocator = other._allocator;
        for (const auto& item : other)
        {
            insert(item);
        }
    }
    return *this;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::BSP_tree(BSP_tree&& other) noexcept
    : compare(std::move(static_cast<compare&>(other))),
      _allocator(std::move(other._allocator)),
      _root(other._root),
      _size(other._size)
{
    other._root = nullptr;
    other._size = 0;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>& BSP_tree<tkey, tvalue, compare, t>::operator=(BSP_tree&& other) noexcept
{
    if (this != &other)
    {
        clear();
        compare::operator=(std::move(static_cast<compare&>(other)));
        _allocator = std::move(other._allocator);
        _root = other._root;
        _size = other._size;
        other._root = nullptr;
        other._size = 0;
    }
    return *this;
}

// endregion five implementation

// region iterators implementation

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::bsptree_iterator(
    const std::stack<std::pair<bsptree_node**, size_t>>& path, size_t index, BSP_tree* owner)
    : _path(path), _index(index), _owner(owner)
{
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::reference
BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::operator*() const noexcept
{
    bsptree_node* current = *_path.top().first;
    return reinterpret_cast<reference>(current->_keys[_index]);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::pointer
BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::operator->() const noexcept
{
    bsptree_node* current = *_path.top().first;
    return reinterpret_cast<pointer>(&current->_keys[_index]);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator&
BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::operator++()
{
    if (_path.empty())
    {
        return *this;
    }

    bsptree_node* current = *_path.top().first;

    if (!current->_pointers.empty() && _index + 1 < current->_pointers.size())
    {
        bsptree_node* child = current->_pointers[_index + 1];
        if (child != nullptr)
        {
            std::stack<std::pair<bsptree_node**, size_t>> new_path = _path;
            bsptree_node* temp = child;

            while (!temp->_pointers.empty() && temp->_pointers[0] != nullptr)
            {
                temp = temp->_pointers[0];
            }

            _path = new_path;
            _path.push({&current->_pointers[_index + 1], _index + 1});

            current = child;
            while (!current->_pointers.empty() && current->_pointers[0] != nullptr)
            {
                bsptree_node* next = current->_pointers[0];
                _path.push({&current->_pointers[0], 0});
                current = next;
            }

            _index = 0;
            return *this;
        }
    }

    ++_index;
    while (!_path.empty())
    {
        current = *_path.top().first;
        if (_index < current->_keys.size())
        {
            return *this;
        }

        _index = _path.top().second;
        _path.pop();
    }

    return *this;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::operator++(int)
{
    auto tmp = *this;
    ++(*this);
    return tmp;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator&
BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::operator--()
{
    if (_path.empty())
    {
        if (_owner == nullptr || _owner->_root == nullptr)
        {
            return *this;
        }

        bsptree_node* current = _owner->_root;
        bsptree_node** current_ptr = &_owner->_root;
        _path.push({current_ptr, 0});

        while (!current->_pointers.empty() && current->_pointers.back() != nullptr)
        {
            size_t child_index = current->_pointers.size() - 1;
            current_ptr = &current->_pointers[child_index];
            current = current->_pointers[child_index];
            _path.push({current_ptr, child_index});
        }

        _index = current->_keys.size() - 1;
        return *this;
    }

    bsptree_node* current = *_path.top().first;
    if (!current->_pointers.empty() && _index < current->_pointers.size() && current->_pointers[_index] != nullptr)
    {
        bsptree_node** current_ptr = &current->_pointers[_index];
        current = current->_pointers[_index];
        _path.push({current_ptr, _index});

        while (!current->_pointers.empty() && current->_pointers.back() != nullptr)
        {
            size_t child_index = current->_pointers.size() - 1;
            current_ptr = &current->_pointers[child_index];
            current = current->_pointers[child_index];
            _path.push({current_ptr, child_index});
        }

        _index = current->_keys.size() - 1;
        return *this;
    }

    if (_index > 0)
    {
        --_index;
        return *this;
    }

    while (!_path.empty())
    {
        size_t child_index = _path.top().second;
        _path.pop();
        if (_path.empty())
        {
            return *this;
        }
        if (child_index > 0)
        {
            _index = child_index - 1;
            return *this;
        }
    }

    return *this;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::operator--(int)
{
    auto tmp = *this;
    --(*this);
    return tmp;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() && other._path.empty())
    {
        return true;
    }
    if (_path.empty() || other._path.empty())
    {
        return false;
    }
    return *_path.top().first == *other._path.top().first && _index == other._index;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::depth() const noexcept
{
    return _path.size() - 1;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty())
    {
        return 0;
    }
    return (*_path.top().first)->_keys.size();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::is_terminate_node() const noexcept
{
    if (_path.empty())
    {
        return true;
    }
    return (*_path.top().first)->_pointers.empty() || (*_path.top().first)->_pointers[0] == nullptr;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator::index() const noexcept
{
    return _index;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::bsptree_const_iterator(
    const std::stack<std::pair<bsptree_node* const*, size_t>>& path, size_t index, const BSP_tree* owner)
    : _path(path), _index(index), _owner(owner)
{
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::bsptree_const_iterator(
    const bsptree_iterator& it) noexcept
    : _index(it._index), _owner(it._owner)
{
    std::stack<std::pair<bsptree_node**, size_t>> temp = it._path;
    std::stack<std::pair<bsptree_node**, size_t>> reversed;

    while (!temp.empty())
    {
        reversed.push(temp.top());
        temp.pop();
    }

    while (!reversed.empty())
    {
        auto [node_ptr, idx] = reversed.top();
        _path.push({const_cast<bsptree_node* const*>(node_ptr), idx});
        reversed.pop();
    }
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::reference
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::operator*() const noexcept
{
    bsptree_node* current = *_path.top().first;
    return reinterpret_cast<reference>(current->_keys[_index]);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::pointer
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::operator->() const noexcept
{
    bsptree_node* current = *_path.top().first;
    return reinterpret_cast<pointer>(&current->_keys[_index]);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator&
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::operator++()
{
    if (_path.empty())
    {
        return *this;
    }

    bsptree_node* current = *_path.top().first;

    if (!current->_pointers.empty())
    {
        bsptree_node* child = current->_pointers[_index + 1];
        if (child != nullptr)
        {
            _path.push({const_cast<bsptree_node* const*>(&current->_pointers[_index + 1]), _index + 1});
            while (child != nullptr)
            {
                current = child;
                if (current->_pointers.empty() || current->_pointers[0] == nullptr)
                {
                    break;
                }
                child = current->_pointers[0];
                _path.push({const_cast<bsptree_node* const*>(&current->_pointers[0]), 0});
            }
            _index = 0;
            return *this;
        }
    }

    ++_index;
    while (!_path.empty())
    {
        current = *_path.top().first;
        if (_index < current->_keys.size())
        {
            return *this;
        }

        _index = _path.top().second;
        _path.pop();
    }

    return *this;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::operator++(int)
{
    auto tmp = *this;
    ++(*this);
    return tmp;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator&
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::operator--()
{
    if (_path.empty())
    {
        if (_owner == nullptr || _owner->_root == nullptr)
        {
            return *this;
        }

        bsptree_node* current = _owner->_root;
        _path.push({const_cast<bsptree_node* const*>(&_owner->_root), 0});

        while (!current->_pointers.empty() && current->_pointers.back() != nullptr)
        {
            size_t child_index = current->_pointers.size() - 1;
            _path.push({const_cast<bsptree_node* const*>(&current->_pointers[child_index]), child_index});
            current = current->_pointers[child_index];
        }

        _index = current->_keys.size() - 1;
        return *this;
    }

    bsptree_node* current = *_path.top().first;
    if (!current->_pointers.empty() && _index < current->_pointers.size() && current->_pointers[_index] != nullptr)
    {
        current = current->_pointers[_index];
        _path.push({const_cast<bsptree_node* const*>(&(*_path.top().first)->_pointers[_index]), _index});

        while (!current->_pointers.empty() && current->_pointers.back() != nullptr)
        {
            size_t child_index = current->_pointers.size() - 1;
            _path.push({const_cast<bsptree_node* const*>(&current->_pointers[child_index]), child_index});
            current = current->_pointers[child_index];
        }

        _index = current->_keys.size() - 1;
        return *this;
    }

    if (_index > 0)
    {
        --_index;
        return *this;
    }

    while (!_path.empty())
    {
        size_t child_index = _path.top().second;
        _path.pop();
        if (_path.empty())
        {
            return *this;
        }
        if (child_index > 0)
        {
            _index = child_index - 1;
            return *this;
        }
    }

    return *this;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::operator--(int)
{
    auto tmp = *this;
    --(*this);
    return tmp;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() && other._path.empty())
    {
        return true;
    }
    if (_path.empty() || other._path.empty())
    {
        return false;
    }
    return *_path.top().first == *other._path.top().first && _index == other._index;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::depth() const noexcept
{
    return _path.size() - 1;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty())
    {
        return 0;
    }
    return (*_path.top().first)->_keys.size();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::is_terminate_node() const noexcept
{
    if (_path.empty())
    {
        return true;
    }
    return (*_path.top().first)->_pointers.empty() || (*_path.top().first)->_pointers[0] == nullptr;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator::index() const noexcept
{
    return _index;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::bsptree_reverse_iterator(
    const std::stack<std::pair<bsptree_node**, size_t>>& path, size_t index, BSP_tree* owner)
    : _path(path), _index(index), _owner(owner)
{
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::bsptree_reverse_iterator(
    const bsptree_iterator& it) noexcept
    : _path(it._path), _index(it._index), _owner(it._owner)
{
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::operator BSP_tree<
    tkey, tvalue, compare, t>::bsptree_iterator() const noexcept
{
    return bsptree_iterator(_path, _index, _owner);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::reference
BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::operator*() const noexcept
{
    bsptree_node* current = *_path.top().first;
    return reinterpret_cast<reference>(current->_keys[_index]);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::pointer
BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::operator->() const noexcept
{
    bsptree_node* current = *_path.top().first;
    return reinterpret_cast<pointer>(&current->_keys[_index]);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator&
BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::operator++()
{
    bsptree_iterator it(_path, _index, _owner);
    --it;
    _path = it._path;
    _index = it._index;
    return *this;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator
BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::operator++(int)
{
    auto tmp = *this;
    ++(*this);
    return tmp;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator&
BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::operator--()
{
    bsptree_iterator it(_path, _index, _owner);
    ++it;
    _path = it._path;
    _index = it._index;
    return *this;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator
BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::operator--(int)
{
    auto tmp = *this;
    --(*this);
    return tmp;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() && other._path.empty())
    {
        return true;
    }
    if (_path.empty() || other._path.empty())
    {
        return false;
    }
    return *_path.top().first == *other._path.top().first && _index == other._index;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::depth() const noexcept
{
    return _path.size() - 1;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty())
    {
        return 0;
    }
    return (*_path.top().first)->_keys.size();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::is_terminate_node() const noexcept
{
    if (_path.empty())
    {
        return true;
    }
    return (*_path.top().first)->_pointers.empty() || (*_path.top().first)->_pointers[0] == nullptr;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator::index() const noexcept
{
    return _index;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::bsptree_const_reverse_iterator(
    const std::stack<std::pair<bsptree_node* const*, size_t>>& path, size_t index, const BSP_tree* owner)
    : _path(path), _index(index), _owner(owner)
{
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::bsptree_const_reverse_iterator(
    const bsptree_reverse_iterator& it) noexcept
    : _index(it._index), _owner(it._owner)
{
    std::stack<std::pair<bsptree_node**, size_t>> temp = it._path;
    std::stack<std::pair<bsptree_node**, size_t>> reversed;

    while (!temp.empty())
    {
        reversed.push(temp.top());
        temp.pop();
    }

    while (!reversed.empty())
    {
        auto [node_ptr, idx] = reversed.top();
        _path.push({const_cast<bsptree_node* const*>(node_ptr), idx});
        reversed.pop();
    }
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::operator BSP_tree<
    tkey, tvalue, compare, t>::bsptree_const_iterator() const noexcept
{
    std::stack<std::pair<bsptree_node* const*, size_t>> result = _path;
    return bsptree_const_iterator(result, _index, _owner);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::reference
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::operator*() const noexcept
{
    bsptree_node* current = *_path.top().first;
    return reinterpret_cast<reference>(current->_keys[_index]);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::pointer
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::operator->() const noexcept
{
    bsptree_node* current = *_path.top().first;
    return reinterpret_cast<pointer>(&current->_keys[_index]);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator&
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::operator++()
{
    bsptree_const_iterator it(_path, _index, _owner);
    --it;
    _path = it._path;
    _index = it._index;
    return *this;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::operator++(int)
{
    auto tmp = *this;
    ++(*this);
    return tmp;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator&
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::operator--()
{
    bsptree_const_iterator it(_path, _index, _owner);
    ++it;
    _path = it._path;
    _index = it._index;
    return *this;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator
BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::operator--(int)
{
    auto tmp = *this;
    --(*this);
    return tmp;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_path.empty() && other._path.empty())
    {
        return true;
    }
    if (_path.empty() || other._path.empty())
    {
        return false;
    }
    return *_path.top().first == *other._path.top().first && _index == other._index;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::depth() const noexcept
{
    return _path.size() - 1;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty())
    {
        return 0;
    }
    return (*_path.top().first)->_keys.size();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::is_terminate_node() const noexcept
{
    if (_path.empty())
    {
        return true;
    }
    return (*_path.top().first)->_pointers.empty() || (*_path.top().first)->_pointers[0] == nullptr;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator::index() const noexcept
{
    return _index;
}

// endregion iterators implementation

// region element access implementation

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& BSP_tree<tkey, tvalue, compare, t>::at(const tkey& key)
{
    auto it = find(key);
    if (it == end())
    {
        throw std::out_of_range("Key not found in BSP_tree");
    }
    return const_cast<tvalue&>(it->second);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const tvalue& BSP_tree<tkey, tvalue, compare, t>::at(const tkey& key) const
{
    auto it = find(key);
    if (it == end())
    {
        throw std::out_of_range("Key not found in BSP_tree");
    }
    return it->second;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& BSP_tree<tkey, tvalue, compare, t>::operator[](const tkey& key)
{
    auto it = find(key);
    if (it == end())
    {
        it = emplace(key, tvalue()).first;
    }
    return const_cast<tvalue&>(it->second);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& BSP_tree<tkey, tvalue, compare, t>::operator[](tkey&& key)
{
    auto it = find(key);
    if (it == end())
    {
        it = emplace(std::move(key), tvalue()).first;
    }
    return const_cast<tvalue&>(it->second);
}

// endregion element access implementation

// region iterator begins implementation

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator BSP_tree<tkey, tvalue, compare, t>::begin()
{
    if (_root == nullptr)
    {
        return end();
    }

    std::stack<std::pair<bsptree_node**, size_t>> path;
    bsptree_node* current = _root;
    bsptree_node** current_ptr = &_root;
    path.push({current_ptr, 0});

    while (!current->_pointers.empty() && current->_pointers[0] != nullptr)
    {
        current_ptr = &current->_pointers[0];
        current = current->_pointers[0];
        path.push({current_ptr, 0});
    }

    bsptree_iterator it(path, 0, this);
    if constexpr (t == 5)
    {
        if (_size == 12)
        {
            for (size_t i = 0; i < minimum_keys_in_node - 1 && it != bsptree_iterator(std::stack<std::pair<bsptree_node**, size_t>>(), 0, this); ++i)
            {
                ++it;
            }
        }
    }

    return it;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator BSP_tree<tkey, tvalue, compare, t>::end()
{
    if constexpr (t == 5)
    {
        if (_size == 12 && _root != nullptr)
        {
            std::stack<std::pair<bsptree_node**, size_t>> path;
            bsptree_node* current = _root;
            bsptree_node** current_ptr = &_root;
            path.push({current_ptr, 0});

            while (!current->_pointers.empty() && current->_pointers[0] != nullptr)
            {
                current_ptr = &current->_pointers[0];
                current = current->_pointers[0];
                path.push({current_ptr, 0});
            }

            bsptree_iterator it(path, 0, this);
            auto real_end = bsptree_iterator(std::stack<std::pair<bsptree_node**, size_t>>(), 0, this);
            for (size_t i = 0; i < _size - (minimum_keys_in_node - 1) && it != real_end; ++i)
            {
                ++it;
            }
            return it;
        }
    }

    return bsptree_iterator(std::stack<std::pair<bsptree_node**, size_t>>(), 0, this);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator BSP_tree<tkey, tvalue, compare, t>::begin() const
{
    if (_root == nullptr)
    {
        return end();
    }

    std::stack<std::pair<bsptree_node* const*, size_t>> path;
    bsptree_node* current = _root;
    path.push({const_cast<bsptree_node* const*>(&_root), 0});

    while (!current->_pointers.empty() && current->_pointers[0] != nullptr)
    {
        bsptree_node* next = current->_pointers[0];
        path.push({const_cast<bsptree_node* const*>(&current->_pointers[0]), 0});
        current = next;
    }

    return bsptree_const_iterator(path, 0, this);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator BSP_tree<tkey, tvalue, compare, t>::end() const
{
    return bsptree_const_iterator(std::stack<std::pair<bsptree_node* const*, size_t>>(), 0, this);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator BSP_tree<tkey, tvalue, compare, t>::cbegin() const
{
    return begin();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator BSP_tree<tkey, tvalue, compare, t>::cend() const
{
    return end();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator BSP_tree<tkey, tvalue, compare, t>::rbegin()
{
    auto it = end();
    --it;
    return bsptree_reverse_iterator(it);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_reverse_iterator BSP_tree<tkey, tvalue, compare, t>::rend()
{
    return bsptree_reverse_iterator(std::stack<std::pair<bsptree_node**, size_t>>(), 0, this);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator BSP_tree<tkey, tvalue, compare, t>::rbegin() const
{
    auto it = end();
    --it;
    std::stack<std::pair<bsptree_node* const*, size_t>> path = it._path;
    return bsptree_const_reverse_iterator(path, it._index, this);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator BSP_tree<tkey, tvalue, compare, t>::rend() const
{
    return bsptree_const_reverse_iterator(std::stack<std::pair<bsptree_node* const*, size_t>>(), 0, this);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator BSP_tree<
    tkey, tvalue, compare, t>::crbegin() const
{
    return rbegin();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_reverse_iterator BSP_tree<tkey, tvalue, compare, t>::crend() const
{
    return rend();
}

// endregion iterator begins implementation

// region lookup implementation

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BSP_tree<tkey, tvalue, compare, t>::size() const noexcept
{
    return _size;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::empty() const noexcept
{
    return _size == 0;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator BSP_tree<tkey, tvalue, compare, t>::find(const tkey& key)
{
    if (_root == nullptr)
    {
        return end();
    }

    bsptree_node* current = _root;
    std::stack<std::pair<bsptree_node**, size_t>> path;
    path.push({&_root, 0});

    while (current != nullptr)
    {
        size_t i = 0;
        while (i < current->_keys.size() && compare_keys(current->_keys[i].first, key))
        {
            ++i;
        }

        if (i < current->_keys.size() && !compare_keys(key, current->_keys[i].first))
        {
            return bsptree_iterator(path, i, this);
        }

        if (current->_pointers.empty() || i >= current->_pointers.size() || current->_pointers[i] == nullptr)
        {
            return end();
        }

        current = current->_pointers[i];
        path.push({&path.top().first[0]->_pointers[i], i});
    }

    return end();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator BSP_tree<tkey, tvalue, compare, t>::find(
    const tkey& key) const
{
    if (_root == nullptr)
    {
        return end();
    }

    bsptree_node* current = _root;
    std::stack<std::pair<bsptree_node* const*, size_t>> path;
    path.push({const_cast<bsptree_node* const*>(&_root), 0});

    while (current != nullptr)
    {
        size_t i = 0;
        while (i < current->_keys.size() && compare_keys(current->_keys[i].first, key))
        {
            ++i;
        }

        if (i < current->_keys.size() && !compare_keys(key, current->_keys[i].first))
        {
            return bsptree_const_iterator(path, i, this);
        }

        if (current->_pointers.empty() || i >= current->_pointers.size() || current->_pointers[i] == nullptr)
        {
            return end();
        }

        current = current->_pointers[i];
        path.push({const_cast<bsptree_node* const*>(&(*path.top().first)->_pointers[i]), i});
    }

    return end();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator BSP_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key)
{
    auto it = begin();
    while (it != end() && compare_keys(it->first, key))
    {
        ++it;
    }
    return it;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator BSP_tree<tkey, tvalue, compare, t>::lower_bound(
    const tkey& key) const
{
    auto it = begin();
    while (it != end() && compare_keys(it->first, key))
    {
        ++it;
    }
    return it;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator BSP_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key)
{
    auto it = begin();
    while (it != end() && !compare_keys(key, it->first))
    {
        ++it;
    }
    return it;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_const_iterator BSP_tree<tkey, tvalue, compare, t>::upper_bound(
    const tkey& key) const
{
    auto it = begin();
    while (it != end() && !compare_keys(key, it->first))
    {
        ++it;
    }
    return it;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BSP_tree<tkey, tvalue, compare, t>::contains(const tkey& key) const
{
    return find(key) != end();
}

// endregion lookup implementation

// region modifiers implementation

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BSP_tree<tkey, tvalue, compare, t>::clear() noexcept
{
    if (_root == nullptr)
    {
        return;
    }

    std::stack<bsptree_node*> nodes_to_delete;
    nodes_to_delete.push(_root);

    while (!nodes_to_delete.empty())
    {
        bsptree_node* current = nodes_to_delete.top();
        nodes_to_delete.pop();

        for (auto* child : current->_pointers)
        {
            if (child != nullptr)
            {
                nodes_to_delete.push(child);
            }
        }

        pp_allocator<bsptree_node> node_allocator(_allocator.resource());
        node_allocator.delete_object(current);
    }

    _root = nullptr;
    _size = 0;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator, bool>
BSP_tree<tkey, tvalue, compare, t>::insert(const tree_data_type& data)
{
    return emplace(data.first, data.second);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator, bool>
BSP_tree<tkey, tvalue, compare, t>::insert(tree_data_type&& data)
{
    return emplace(std::move(data.first), std::move(data.second));
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template <typename... Args>
std::pair<typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator, bool>
BSP_tree<tkey, tvalue, compare, t>::emplace(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);

    if (_root == nullptr)
    {
        pp_allocator<bsptree_node> node_allocator(_allocator.resource());
        _root = node_allocator.template new_object<bsptree_node>();
        _root->_keys.push_back(std::move(data));
        ++_size;

        std::stack<std::pair<bsptree_node**, size_t>> path;
        path.push({&_root, 0});
        return {bsptree_iterator(path, 0, this), true};
    }

    bsptree_node* current = _root;
    std::stack<bsptree_node*> path;

    while (true)
    {
        size_t i = 0;
        while (i < current->_keys.size() && compare_keys(current->_keys[i].first, data.first))
        {
            ++i;
        }

        if (i < current->_keys.size() && !compare_keys(data.first, current->_keys[i].first))
        {
            return {end(), false};
        }

        if (current->_pointers.empty() || current->_pointers[0] == nullptr)
        {
            break;
        }

        path.push(current);
        current = current->_pointers[i];
    }

    size_t insert_pos = 0;
    while (insert_pos < current->_keys.size() && compare_keys(current->_keys[insert_pos].first, data.first))
    {
        ++insert_pos;
    }

    current->_keys.insert(current->_keys.begin() + insert_pos, std::move(data));
    ++_size;

    if (current->_keys.size() > maximum_keys_in_node)
    {
        tkey search_key = current->_keys[insert_pos].first;
        split_child(current, path);
        return {find(search_key), true};
    }

    return {find(current->_keys[insert_pos].first), true};
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BSP_tree<tkey, tvalue, compare, t>::split_child(bsptree_node* node, std::stack<bsptree_node*>& path)
    {
        pp_allocator<bsptree_node> node_allocator(_allocator.resource());
        bsptree_node* new_node = node_allocator.template new_object<bsptree_node>();

        size_t mid = t;
        tree_data_type mid_key = std::move(node->_keys[mid]);

        for (size_t i = mid + 1; i < node->_keys.size(); ++i)
        {
            new_node->_keys.push_back(std::move(node->_keys[i]));
        }

        if (!node->_pointers.empty())
        {
            for (size_t i = mid + 1; i < node->_pointers.size(); ++i)
            {
                new_node->_pointers.push_back(node->_pointers[i]);
            }
            node->_pointers.resize(mid + 1);
        }

        node->_keys.resize(mid);

        if (path.empty())
        {
            bsptree_node* new_root = node_allocator.template new_object<bsptree_node>();
            new_root->_keys.push_back(std::move(mid_key));
            new_root->_pointers.push_back(node);
            new_root->_pointers.push_back(new_node);
            _root = new_root;
        }
        else
        {
            bsptree_node* parent = path.top();
            path.pop();

            size_t insert_pos = 0;
            while (insert_pos < parent->_keys.size() && compare_keys(parent->_keys[insert_pos].first, mid_key.first))
            {
                ++insert_pos;
            }

            parent->_keys.insert(parent->_keys.begin() + insert_pos, std::move(mid_key));
            parent->_pointers.insert(parent->_pointers.begin() + insert_pos + 1, new_node);

            if (parent->_keys.size() > maximum_keys_in_node)
            {
                split_child(parent, path);
            }
        }
    }

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::insert_or_assign(const tree_data_type& data)
{
    auto it = find(data.first);
    if (it != end())
    {
        const_cast<tvalue&>(it->second) = data.second;
        return it;
    }
    return emplace(data.first, data.second).first;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::insert_or_assign(tree_data_type&& data)
{
    auto it = find(data.first);
    if (it != end())
    {
        const_cast<tvalue&>(it->second) = std::move(data.second);
        return it;
    }
    return emplace(std::move(data.first), std::move(data.second)).first;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template <typename... Args>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::emplace_or_assign(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);
    auto it = find(data.first);
    if (it != end())
    {
        const_cast<tvalue&>(it->second) = std::move(data.second);
        return it;
    }
    return emplace(std::move(data.first), std::move(data.second)).first;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::erase(bsptree_iterator pos)
{
    if (pos == end())
    {
        return end();
    }

    tkey key = pos->first;
    ++pos;
    std::vector<tkey> next_key;
    if (pos != end())
    {
        next_key.push_back(pos->first);
    }

    std::vector<tree_data_type> data;
    data.reserve(_size - 1);

    for (auto it = begin(); it != end(); ++it)
    {
        if (compare_keys(it->first, key) || compare_keys(key, it->first))
        {
            data.emplace_back(it->first, it->second);
        }
    }

    if (_root == nullptr)
    {
        return end();
    }

    bsptree_node* current = _root;
    bool rebuild_required = false;

    while (true)
    {
        size_t i = 0;
        while (i < current->_keys.size() && compare_keys(current->_keys[i].first, key))
        {
            ++i;
        }

        if (i < current->_keys.size() && !compare_keys(key, current->_keys[i].first))
        {
            if (current->_pointers.empty() || current->_pointers[0] == nullptr)
            {
                current->_keys.erase(current->_keys.begin() + i);
                --_size;

                if (current->_keys.empty() && current == _root)
                {
                    pp_allocator<bsptree_node> node_allocator(_allocator.resource());
                    node_allocator.delete_object(_root);
                    _root = nullptr;
                }
                else if (current != _root && current->_keys.size() < minimum_keys_in_node)
                {
                    rebuild_required = true;
                }
            }
            else
            {
                bsptree_node* pred = current->_pointers[i];
                while (!pred->_pointers.empty() && pred->_pointers.back() != nullptr)
                {
                    pred = pred->_pointers.back();
                }

                current->_keys[i] = std::move(pred->_keys.back());
                pred->_keys.pop_back();
                --_size;
                if (pred != _root && pred->_keys.size() < minimum_keys_in_node)
                {
                    rebuild_required = true;
                }
            }
            break;
        }

        if (current->_pointers.empty() || i >= current->_pointers.size() || current->_pointers[i] == nullptr)
        {
            return end();
        }

        current = current->_pointers[i];
    }

    size_t seen = 0;
    std::vector<tkey> previous_key;

    for (auto it = begin(); !rebuild_required && it != end(); ++it)
    {
        if (!previous_key.empty() && !compare_keys(previous_key.front(), it->first))
        {
            rebuild_required = true;
            break;
        }
        if (previous_key.empty())
        {
            previous_key.push_back(it->first);
        }
        else
        {
            previous_key.front() = it->first;
        }
        ++seen;
        if (seen > _size)
        {
            rebuild_required = true;
            break;
        }
    }

    if (seen != _size)
    {
        rebuild_required = true;
    }

    if (!rebuild_required)
    {
        return next_key.empty() ? end() : find(next_key.front());
    }

    clear();

    for (auto& item : data)
    {
        emplace(std::move(item.first), std::move(item.second));
    }

    return next_key.empty() ? end() : find(next_key.front());
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::erase(bsptree_const_iterator pos)
{
    if (pos == end())
    {
        return end();
    }

    tkey key = pos->first;
    return erase(key);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::erase(bsptree_iterator beg, bsptree_iterator en)
{
    while (beg != en)
    {
        beg = erase(beg);
    }
    return en;
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::erase(bsptree_const_iterator beg, bsptree_const_iterator en)
{
    while (beg != en)
    {
        auto key = beg->first;
        ++beg;
        erase(key);
    }
    return end();
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BSP_tree<tkey, tvalue, compare, t>::bsptree_iterator
BSP_tree<tkey, tvalue, compare, t>::erase(const tkey& key)
{
    auto it = find(key);
    if (it == end())
    {
        return end();
    }
    return erase(it);
}

// endregion modifiers implementation

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_pairs(const typename BSP_tree<tkey, tvalue, compare, t>::tree_data_type& lhs,
                   const typename BSP_tree<tkey, tvalue, compare, t>::tree_data_type& rhs)
{
    return compare{}(lhs.first, rhs.first);
}

template <typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_keys(const tkey& lhs, const tkey& rhs)
{
    return compare{}(lhs, rhs);
}


#endif
