#pragma once

#include <algorithm>

namespace eng {

template<typename T, typename Comp = std::less<>> class SortedVector {
public:
    SortedVector() = default;
    explicit SortedVector(std::initializer_list<int> values): _data(values) {
        std::sort(begin(), end(), Comp{});
    }
    explicit SortedVector(const std::vector<T> &v): _data(v) {
        std::sort(begin(), end(), Comp{});
    }
    explicit SortedVector(std::vector<T> &&v): _data(std::move(v)) {
        std::sort(begin(), end(), Comp{});
    }
    SortedVector& operator=(const std::vector<T> &v) noexcept {
        _data = v;
        std::sort(begin(), end(), Comp{});
    }
    SortedVector& operator=(std::vector<T> &&v) noexcept {
        _data = std::move(v);
        std::sort(begin(), end(), Comp{});
    }

    constexpr void push(const T& value) {
        auto it = std::upper_bound(begin(), end(), value, Comp{});
        _data.insert(it, value);
    }
    constexpr void push(T&& value) {
        auto it = std::upper_bound(begin(), end(), value, Comp{});
        _data.emplace(it, std::move(value));
    }

    constexpr typename std::vector<T>::iterator begin() noexcept { return _data.begin(); }
    constexpr typename std::vector<T>::iterator end() noexcept { return _data.end(); }
    constexpr typename std::vector<T>::const_iterator begin() const noexcept { return _data.begin(); }
    constexpr typename std::vector<T>::const_iterator end() const noexcept { return _data.end(); }
    constexpr typename std::vector<T>::const_iterator cbegin() const noexcept { return _data.cbegin(); }
    constexpr typename std::vector<T>::const_iterator cend() const noexcept { return _data.cend(); }
    constexpr typename std::vector<T>::size_type size() const noexcept { return _data.size(); }
    constexpr T& at(typename std::vector<T>::size_type pos) { return _data.at(pos); }
    constexpr const T& at(typename std::vector<T>::size_type pos) const { return _data.at(pos); }
    constexpr T& operator[](typename std::vector<T>::size_type pos) { return _data[pos]; }
    constexpr const T& operator[](typename std::vector<T>::size_type pos) const { return _data[pos]; }

private:
    std::vector<T> _data;
};

}