#pragma once
#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

namespace vd {
/**
 * @usage
 * vd::Arena arena(1024 * 64); // Initialize with 64KB chunks
 * no need to call delete; arena owns the memory.
 * auto* node = arena.alloc<AstNode>(NodeKind::BinaryExpr, left, right);
 * reset the arena (reuse memory for the next pass):
 * warning: this does not call destructors. use only for pod types.
 * arena.reset();
 * all allocated memory is freed when the arena object goes out of scope.
 */
class Arena {
private:
  struct Chunk {
    std::unique_ptr<std::byte[]> storage;
    std::size_t size;
  };

  std::vector<Chunk> chunks_;
  std::size_t default_chunk_size_;
  std::byte *current_ptr_ = nullptr;
  std::byte *end_ptr_ = nullptr;

  void grow(std::size_t min_size) {
    std::size_t size_to_alloc = std::max(default_chunk_size_, min_size);
    auto storage = std::make_unique<std::byte[]>(size_to_alloc);
    std::byte *raw_ptr = storage.get();

    chunks_.push_back({std::move(storage), size_to_alloc});

    current_ptr_ = raw_ptr;
    end_ptr_ = current_ptr_ + size_to_alloc;
  }

public:
  explicit Arena(std::size_t chunk_size = 64 * 1024)
      : default_chunk_size_(chunk_size) {}

  template <typename T, typename... Args> T *alloc(Args &&...args) {
    std::size_t size = sizeof(T);
    std::size_t alignment = alignof(T);

    // Cast to size_t to prevent the signed/unsigned warning
    std::size_t space = (current_ptr_ == nullptr)
                            ? 0
                            : static_cast<std::size_t>(end_ptr_ - current_ptr_);
    void *scratch = current_ptr_;

    if (current_ptr_ == nullptr ||
        !std::align(alignment, size, scratch, space)) {
      grow(size);
      scratch = current_ptr_;
      space = static_cast<std::size_t>(end_ptr_ - current_ptr_);
      std::align(alignment, size, scratch, space);
    }

    current_ptr_ = static_cast<std::byte *>(scratch) + size;
    return new (scratch) T(std::forward<Args>(args)...);
  }

  void reset() {
    if (chunks_.empty())
      return;
    current_ptr_ = chunks_[0].storage.get();
    end_ptr_ = current_ptr_ + chunks_[0].size;
  }
};
} // namespace vd
