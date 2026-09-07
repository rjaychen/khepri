#pragma once

#include <memory>
#include <type_traits>
#include <cassert>

namespace khepri {

template <typename T>
class SharedOrObserved {
public:
    using element_type = T;

    constexpr SharedOrObserved() noexcept : m_ptr(nullptr) {}
    constexpr SharedOrObserved(std::nullptr_t) noexcept : m_ptr(nullptr) {}

    explicit SharedOrObserved(T* rawPtr) noexcept : m_ptr(rawPtr) {}

    /* implicit */ SharedOrObserved(std::shared_ptr<T> sharedPtr) noexcept
        : m_shared(std::move(sharedPtr)), m_ptr(m_shared.get()) {}

    template <typename U>
        requires std::is_convertible_v<U*, T*>
    /* implicit */ SharedOrObserved(const SharedOrObserved<U>& other) noexcept
        : m_shared(other.m_shared), m_ptr(other.m_ptr) {}

    template <typename U>
        requires std::is_convertible_v<U*, T*>
    /* implicit */ SharedOrObserved(SharedOrObserved<U>&& other) noexcept
        : m_shared(std::move(other.m_shared)), m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    SharedOrObserved(const SharedOrObserved&) = default;
    SharedOrObserved(SharedOrObserved&&) noexcept = default;
    SharedOrObserved& operator=(const SharedOrObserved&) = default;
    SharedOrObserved& operator=(SharedOrObserved&&) noexcept = default;

    SharedOrObserved& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    SharedOrObserved& operator=(T* rawPtr) noexcept {
        m_shared.reset();
        m_ptr = rawPtr;
        return *this;
    }

    SharedOrObserved& operator=(std::shared_ptr<T> sharedPtr) noexcept {
        m_shared = std::move(sharedPtr);
        m_ptr = m_shared.get();
        return *this;
    }

    [[nodiscard]] bool is_owning() const noexcept {
        return m_shared != nullptr;
    }

    [[nodiscard]] long use_count() const noexcept {
        return m_shared ? m_shared.use_count() : 0;
    }

    [[nodiscard]] bool is_null() const noexcept {
        return m_ptr == nullptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    [[nodiscard]] T* get() const noexcept {
        return m_ptr;
    }

    [[nodiscard]] T* operator->() const noexcept {
        assert(m_ptr != nullptr && "Dereferencing null SharedOrObserved pointer");
        return m_ptr;
    }

    [[nodiscard]] T& operator*() const noexcept {
        assert(m_ptr != nullptr && "Dereferencing null SharedOrObserved pointer");
        return *m_ptr;
    }

    [[nodiscard]] const std::shared_ptr<T>& get_shared() const noexcept {
        return m_shared;
    }

    void reset() noexcept {
        m_shared.reset();
        m_ptr = nullptr;
    }

    template <typename U>
    [[nodiscard]] bool operator==(const SharedOrObserved<U>& other) const noexcept {
        return m_ptr == other.get();
    }

    [[nodiscard]] bool operator==(std::nullptr_t) const noexcept {
        return m_ptr == nullptr;
    }

    template <typename U>
    [[nodiscard]] bool operator==(const U* rawPtr) const noexcept {
        return m_ptr == rawPtr;
    }

    template <typename U>
    [[nodiscard]] bool operator==(const std::shared_ptr<U>& sharedPtr) const noexcept {
        return m_ptr == sharedPtr.get();
    }

private:
    template <typename U>
    friend class SharedOrObserved;

    std::shared_ptr<T> m_shared = nullptr;
    T* m_ptr = nullptr;
};

template <typename T>
SharedOrObserved<T> MakeShared(auto&&... args) {
    return SharedOrObserved<T>(std::make_shared<T>(std::forward<decltype(args)>(args)...));
}

template <typename T>
SharedOrObserved<T> Observe(T* rawPtr) {
    return SharedOrObserved<T>(rawPtr);
}

} // namespace khepri
