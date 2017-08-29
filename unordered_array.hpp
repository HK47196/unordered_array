/*
 * unordered_array.hpp Copyright © 2017 rsw0x
 *
 * Distributed under terms of the MPLv2 license.
 */

#pragma once
#ifndef UNORDERED_ARRAY_HPP82110
#define UNORDERED_ARRAY_HPP82110

////////////////////////////////////////////////////////////////////////////////
//  TODO:
//    Lazy erase implementation:
//      Instead of doing a swap and pop, just mark areas as erasable.
//    Pull out the non-templated pieces and make it a base class.
////////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#if !defined(NDEBUG) && !defined(PEP_DEBUG)
#define PEP_DEBUG 1
#endif

#ifndef PEP_ASSERT
#define PEP_ASSERT(x) assert(x)
#endif

namespace nonstd {

[[noreturn]] inline void unreachable() noexcept {
#if __GNUC__
  __builtin_unreachable();
#else
  std::abort();
#endif
}

struct key_t {
  std::ptrdiff_t key;
};

// I'd really like to be able to do this without templating it on the allocator.

template <typename IndexType>
class unordered_array_impl_ {
protected:
  using size_type = ptrdiff_t;
  using difference_type = ptrdiff_t;

  size_type size_{0};

public:
  size_type size() const noexcept { return size_; }
};

// template <typename T, std::size_t PerBlock = 32, typename Allocator = std::allocator<T>>
template <typename T, typename Container = std::vector<T>, typename IndexType = std::int32_t>
class unordered_array {
  static_assert(std::is_signed<IndexType>::value, "");

  enum index_values : IndexType {
    INVALID = -1,
    INVALID_DENSE = -2,
  };

public:
  using value_type = T;
  using size_type = ptrdiff_t;
  using difference_type = ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = typename Container::iterator;
  using const_iterator = typename Container::const_iterator;

  unordered_array() = default;
  template <typename Allocator>
  unordered_array(const Allocator&);
  unordered_array(unordered_array&& other);
  unordered_array(const unordered_array& other) = delete;
  unordered_array& operator=(unordered_array&& other);
  unordered_array& operator=(const unordered_array& other) = delete;
  ~unordered_array() = default;

  template <typename... Args>
  key_t emplace(Args&&... args);
  key_t insert(const value_type& value);
  key_t insert(value_type&& value);
  void clear() noexcept;

  void shuffle(const key_t& from, const key_t& to) noexcept;

  void erase(const key_t& pos) noexcept(false) /* TODO */;
  reference operator[](const key_t& key) noexcept;
  const_reference operator[](const key_t& key) const noexcept;
  reference at(const key_t& key);
  const_reference at(const key_t& key) const;
  size_type count(const key_t& key) noexcept;
  size_type count(const key_t& key) const noexcept;
  iterator find(const key_t& key) noexcept;
  const_iterator find(const key_t& key) const noexcept;

  void swap(unordered_array& other);

  iterator begin() noexcept;
  const_iterator begin() const noexcept;
  const_iterator cbegin() const noexcept;
  iterator end() noexcept;
  const_iterator end() const noexcept;
  const_iterator cend() const noexcept;

  size_type size() const noexcept;
  bool empty() const noexcept;
  void reserve(size_type new_cap) noexcept;

private:
  template <typename U>
  using vector_alias = std::vector<
    U,
    typename std::allocator_traits<typename Container::allocator_type>::template rebind_alloc<U>>;

  // This can be thought of as a structure of arrays. The two items are conceptually unrelated.
  struct map_item_t {
    IndexType sparse_to_dense;
    IndexType dense_to_sparse;

    map_item_t(const IndexType a, const IndexType b)
      : sparse_to_dense(a)
      , dense_to_sparse(b) {}
  };

  Container storage_;
  vector_alias<map_item_t> map_array_;
  size_type free_slot_{INVALID};
  difference_type size_{0};

  void pop_back_() noexcept;
  bool is_valid_index(size_type idx) const noexcept;
  void debug_valid_index(size_type idx) const noexcept;
};

template <typename T, typename U, typename K>
template <typename Allocator>
unordered_array<T, U, K>::unordered_array(const Allocator& alloc)
  : storage_{alloc} {
}
template <typename T, typename U, typename K>
unordered_array<T, U, K>::unordered_array(unordered_array&& other)
  : storage_{std::move(other.storage)}
  , map_array_{std::move(other.map_array_)}
  , free_slot_{std::exchange(other.free_slot_, INVALID)}
  , size_{std::exchange(other.size_, 0)} {
}
template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::operator=(unordered_array&& other) -> unordered_array& {
  // Don't delay the destruction of the contents, force it now.
  unordered_array tmp{std::move(other)};
  swap(tmp);
  return *this;
}

template <typename T, typename U, typename IndexType>
template <typename... Args>
auto unordered_array<T, U, IndexType>::emplace(Args&&... args) -> key_t {
  // get the index we need into the storage_ array
  const auto idx = [&]() -> difference_type {
    // dense index of our new element TODO: when lazy is implemented it won't always be the end
    const auto dense_idx = storage_.size();
    if (free_slot_ == index_values::INVALID) {
      PEP_ASSERT(size() != std::numeric_limits<IndexType>::max());
      // TODO: bulk grow?
      // indirect index is just the last slot
      map_array_.emplace_back(dense_idx, dense_idx);
      return size();
    } else {
      // Note that there's no real performance downside to using the array as a free-list as it's
      // just touching the same memory
      PEP_ASSERT(!is_valid_index(free_slot_));
      const auto sparse_idx = free_slot_;
      // the sparse_to_dense member is either -1 or the next free slot
      free_slot_ = map_array_[sparse_idx].sparse_to_dense;
      // Debug check to make sure there wasn't a valid index there
      PEP_ASSERT(!is_valid_index(free_slot_));
      map_array_[sparse_idx].sparse_to_dense = dense_idx;
      PEP_ASSERT(dense_idx < map_array_.size());
      PEP_ASSERT(map_array_[dense_idx].dense_to_sparse == INVALID_DENSE);
      map_array_[dense_idx].dense_to_sparse = sparse_idx;
      return sparse_idx;
    }
  }();
  storage_.emplace_back(std::forward<Args>(args)...);
  ++size_;
  debug_valid_index(idx);
  return {idx};
}

template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::insert(const value_type& value) -> key_t {
  return emplace(value);
}

template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::insert(value_type&& value) -> key_t {
  return emplace(std::move(value));
}

template <typename T, typename U, typename K>
void unordered_array<T, U, K>::clear() noexcept {
  storage_.clear();
  map_array_.clear();
  size_ = 0;
  free_slot_ = INVALID;
}

template <typename T, typename U, typename K>
void unordered_array<T, U, K>::shuffle(const key_t& from_k, const key_t& to_k) noexcept {
  debug_valid_index(from_k.key);
  debug_valid_index(to_k.key);

#if PEP_DEBUG
  void* from_addr = &(operator[](from_k));
  void* to_addr = &(operator[](to_k));
#endif
  const auto from = map_array_[from_k.key].sparse_to_dense;
  const auto to = map_array_[to_k.key].sparse_to_dense;
  PEP_ASSERT(map_array_[from].dense_to_sparse != INVALID_DENSE);
  PEP_ASSERT(map_array_[to].dense_to_sparse != INVALID_DENSE);
  std::swap(map_array_[from].dense_to_sparse, map_array_[to].dense_to_sparse);
  std::swap(map_array_[from_k.key].sparse_to_dense, map_array_[to_k.key].sparse_to_dense);

  debug_valid_index(from_k.key);
  debug_valid_index(to_k.key);
#if PEP_DEBUG
  PEP_ASSERT(from_addr == &(operator[](to_k)));
  PEP_ASSERT(to_addr == &(operator[](from_k)));
#endif
  // PEP_ASSERT(from == map_array_[to_k.key].sparse_to_dense);
}

template <typename T, typename U, typename K>
void unordered_array<T, U, K>::pop_back_() noexcept {
  PEP_ASSERT(size() > 0);
  PEP_ASSERT(storage_.size() > 0);
  storage_.pop_back();
  --size_;

  const auto sparse_idx = map_array_[storage_.size()].dense_to_sparse;
  debug_valid_index(sparse_idx);
  map_array_[sparse_idx].sparse_to_dense = free_slot_;
  free_slot_ = sparse_idx;
  map_array_[storage_.size()].dense_to_sparse = INVALID_DENSE;
  PEP_ASSERT(!is_valid_index(sparse_idx));
}

// TODO: investigate if it's possible to erase lazily

template <typename T, typename U, typename K>
void unordered_array<T, U, K>::erase(const key_t& pos) noexcept(false) {
  const auto idx = pos.key;
  debug_valid_index(idx);
  if (idx < 0) {
    unreachable();
  }
  const auto target_sparse_to_dense = map_array_[idx].sparse_to_dense;
  // Rewrite it in terms of pop_back_ to simplify
  const ptrdiff_t end_idx = storage_.size() - 1;
  if (target_sparse_to_dense != end_idx) { // make sure it's not the last element.
    // shuffle & swap
    std::swap(storage_[target_sparse_to_dense], storage_.back()); // <-- might throw
    const auto sparse_index_back = map_array_[end_idx].dense_to_sparse;
    debug_valid_index(sparse_index_back);
    debug_valid_index(idx);
    shuffle(key_t{idx}, key_t{sparse_index_back});
  }
  debug_valid_index(idx);
  PEP_ASSERT(map_array_[idx].sparse_to_dense == end_idx);
  pop_back_();
}

template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::size() const noexcept -> size_type {
  return size_;
}

template <typename T, typename U, typename K>
bool unordered_array<T, U, K>::empty() const noexcept {
  return size() == 0;
}

template <typename T, typename U, typename K>
void unordered_array<T, U, K>::reserve(size_type new_cap) noexcept {
  storage_.reserve(new_cap);
  map_array_.reserve(new_cap);
}

template <typename T, typename U, typename K>
inline auto unordered_array<T, U, K>::operator[](const key_t& key) noexcept -> reference {
  const auto idx = key.key;
  debug_valid_index(idx);
  return storage_[map_array_[idx].sparse_to_dense];
}

template <typename T, typename U, typename K>
inline auto unordered_array<T, U, K>::operator[](const key_t& key) const noexcept
  -> const_reference {
  const auto idx = key.key;
  debug_valid_index(idx);
  return storage_[map_array_[idx].sparse_to_dense];
}

template <typename T, typename U, typename K>
inline auto unordered_array<T, U, K>::at(const key_t& key) -> reference {
  if (!is_valid_index(key.key)) {
    throw std::out_of_range("Invalid index.");
  }
  return (*this)[key];
}

template <typename T, typename U, typename K>
inline auto unordered_array<T, U, K>::at(const key_t& key) const -> const_reference {
  if (!is_valid_index(key.key)) {
    throw std::out_of_range("Invalid index.");
  }
  return (*this)[key];
}

template <typename T, typename U, typename K>
inline auto unordered_array<T, U, K>::count(const key_t& key) noexcept -> size_type {
  return is_valid_index(key.key) ? 1 : 0;
}

template <typename T, typename U, typename K>
inline auto unordered_array<T, U, K>::count(const key_t& key) const noexcept -> size_type {
  return is_valid_index(key.key) ? 1 : 0;
}

template <typename T, typename U, typename K>
inline auto unordered_array<T, U, K>::find(const key_t& key) noexcept -> iterator {
  if (!is_valid_index(key.key)) {
    return storage_.end();
  }
  const auto dense_idx = map_array_[key.key].sparse_to_dense;
  return storage_.begin() + dense_idx;
}

template <typename T, typename U, typename K>
inline auto unordered_array<T, U, K>::find(const key_t& key) const noexcept -> const_iterator {
  if (!is_valid_index(key.key)) {
    return storage_.end();
  }
  const auto dense_idx = map_array_[key.key].sparse_to_dense;
  return storage_.begin() + dense_idx;
}

template <typename T, typename U, typename K>
void unordered_array<T, U, K>::swap(unordered_array<T, U, K>& other) {
  // TODO
  return std::swap(*this, other);
}

template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::begin() noexcept -> iterator {
  return storage_.begin();
}
template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::begin() const noexcept -> const_iterator {
  return cbegin();
}
template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::cbegin() const noexcept -> const_iterator {
  return storage_.cbegin();
}
template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::end() noexcept -> iterator {
  return storage_.end();
}
template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::end() const noexcept -> const_iterator {
  return cend();
}
template <typename T, typename U, typename K>
auto unordered_array<T, U, K>::cend() const noexcept -> const_iterator {
  return storage_.cend();
}

////////////////////////////////////////////////////////////////////////////////
//  For a sparse index to be valid it must have its value in the dense array point back towards
//  itself e.g,
//
//  valid(idx) => dense[sparse[idx]] = idx
////////////////////////////////////////////////////////////////////////////////

template <typename T, typename U, typename K>
bool unordered_array<T, U, K>::is_valid_index(const size_type s_idx) const noexcept {
  if (s_idx < 0) {
    return false;
  }
  const std::size_t idx = std::size_t(s_idx);
  if (idx >= map_array_.size()) {
    return false;
  }
  const auto sparse_to_dense = map_array_[idx].sparse_to_dense;
  if (sparse_to_dense < 0 || std::size_t(sparse_to_dense) >= map_array_.size()) {
    return false;
  }
  const auto dense_to_sparse = map_array_[sparse_to_dense].dense_to_sparse;
  if (s_idx != dense_to_sparse) {
    return false;
  }
  return true;
}

template <typename T, typename U, typename K>
void unordered_array<T, U, K>::debug_valid_index(size_type idx) const noexcept {
#ifdef PEP_DEBUG
  PEP_ASSERT(idx < ptrdiff_t(map_array_.size()));
  const auto sparse_to_dense = map_array_[idx].sparse_to_dense;
  PEP_ASSERT(sparse_to_dense < ptrdiff_t(map_array_.size()));
  const auto dense_to_sparse = map_array_[sparse_to_dense].dense_to_sparse;
  PEP_ASSERT(idx == dense_to_sparse);
#endif
}

} // namespace nonstd

#undef PEP_ASSERT
#undef PEP_DEBUG

#endif /* !UNORDERED_ARRAY_HPP82110 */
