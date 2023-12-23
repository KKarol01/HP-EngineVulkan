#pragma once

#include <atomic>

namespace eng {

inline constexpr struct GENERATE_HANDLE_T{} GENERATE_HANDLE;

template<typename T> struct HandleGenerator;

template<typename T> struct Handle {
    constexpr Handle() = default;
    constexpr Handle(GENERATE_HANDLE_T);
    constexpr Handle(const Handle&) noexcept = default;
    constexpr Handle& operator=(const Handle&) noexcept = default;
    constexpr Handle(Handle &&other) noexcept : handle(other.handle) { other.handle = 0; }
    constexpr Handle& operator=(Handle &&other) { handle = other.handle; other.handle = 0; return *this; }
    virtual constexpr ~Handle() = default;

    constexpr auto operator<=>(const Handle &other) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return handle != 0; }

    size_t handle{0};

private:
    friend struct HandleGenerator<T>;

    constexpr Handle(size_t handle): handle(handle) { }
};

template<typename T> struct HandleGenerator {
    [[nodiscard]] static constexpr Handle<T> generate() noexcept { return Handle<T>{++_handle}; }
    
private:
    inline static std::atomic_size_t _handle{0};
};

template<typename T> constexpr Handle<T>::Handle(GENERATE_HANDLE_T): Handle(HandleGenerator<T>::generate()) { };

}

template<typename T> struct std::hash<eng::Handle<T>> {
    constexpr size_t operator()(const eng::Handle<T> &h) const noexcept { return h.handle; }
};