#pragma once

#include <utility>

namespace apollo::core::di {

using DestroyFn = void (*)(void*);

template <typename T>
class UniqueBean {
public:
    UniqueBean() = default;

    UniqueBean(void* impl_ptr, T* view_ptr, DestroyFn destroy) noexcept
        : impl_ptr_(impl_ptr), view_ptr_(view_ptr), destroy_(destroy) {}

    UniqueBean(const UniqueBean&) = delete;
    UniqueBean& operator=(const UniqueBean&) = delete;

    UniqueBean(UniqueBean&& other) noexcept {
        *this = std::move(other);
    }

    UniqueBean& operator=(UniqueBean&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        reset();
        impl_ptr_ = other.impl_ptr_;
        view_ptr_ = other.view_ptr_;
        destroy_ = other.destroy_;
        other.impl_ptr_ = nullptr;
        other.view_ptr_ = nullptr;
        other.destroy_ = nullptr;
        return *this;
    }

    ~UniqueBean() {
        reset();
    }

    T* get() const noexcept {
        return view_ptr_;
    }

    T& operator*() const {
        return *view_ptr_;
    }

    T* operator->() const noexcept {
        return view_ptr_;
    }

    explicit operator bool() const noexcept {
        return view_ptr_ != nullptr;
    }

    void reset() noexcept {
        if (impl_ptr_ != nullptr && destroy_ != nullptr) {
            destroy_(impl_ptr_);
        }
        impl_ptr_ = nullptr;
        view_ptr_ = nullptr;
        destroy_ = nullptr;
    }

private:
    void* impl_ptr_ = nullptr;
    T* view_ptr_ = nullptr;
    DestroyFn destroy_ = nullptr;
};

} // namespace apollo::core::di
