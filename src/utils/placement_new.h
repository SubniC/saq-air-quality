#pragma once
//mini template wrapper for placement new operations

template <class T>
class InPlace {
  alignas(T) unsigned char mem_[sizeof(T)];
  T* ptr_ = nullptr;

public:
  InPlace() = default;
  ~InPlace() { destroy(); }

  InPlace(const InPlace&) = delete;
  InPlace& operator=(const InPlace&) = delete;

  template <class... Args>
  T* construct(Args&&... args) {
    destroy();
    ptr_ = new (mem_) T(std::forward<Args>(args)...);
    return ptr_;
  }

  void destroy() {
    if (ptr_) { ptr_->~T(); ptr_ = nullptr; }
  }

  T* get()       { return ptr_; }
  T& operator*() { return *ptr_; }
  T* operator->(){ return ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }
};
