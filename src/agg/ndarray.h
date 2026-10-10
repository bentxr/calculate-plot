#pragma once

// A small owning N-dimensional array with the interface matplotlib's renderer templates expect from
// NumPy array views (shape(i), size(), ndim(), operator()(i, j, k), data(i, j, k)), so the ported
// renderer code compiles unchanged around it.

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace mpl {

template <typename T, int ND>
class NdArray {
public:
    NdArray() { shape_.fill(0); }
    explicit NdArray(std::array<std::size_t, ND> shape) : shape_(shape) { values_.resize(count()); }
    NdArray(std::array<std::size_t, ND> shape, std::vector<T> values) : shape_(shape), values_(std::move(values)) {}

    std::size_t shape(std::size_t i) const { return i < std::size_t(ND) ? shape_[i] : 0; }
    std::size_t size() const { return values_.size(); }
    int ndim() const { return ND; }

    T& operator()(std::size_t i, std::size_t j = 0, std::size_t k = 0) { return values_[index(i, j, k)]; }
    const T& operator()(std::size_t i, std::size_t j = 0, std::size_t k = 0) const { return values_[index(i, j, k)]; }
    T* data(std::size_t i = 0, std::size_t j = 0, std::size_t k = 0) { return values_.data() + index(i, j, k); }
    const T* data(std::size_t i = 0, std::size_t j = 0, std::size_t k = 0) const { return values_.data() + index(i, j, k); }

    T* mutable_data(std::size_t i = 0, std::size_t j = 0, std::size_t k = 0) { return data(i, j, k); }

    std::vector<T>& values() { return values_; }
    const std::vector<T>& values() const { return values_; }

private:
    std::size_t count() const {
        std::size_t n = 1;
        for (std::size_t s : shape_) n *= s;
        return n;
    }
    std::size_t index(std::size_t i, std::size_t j, std::size_t k) const {
        if constexpr (ND == 1) return i;
        else if constexpr (ND == 2) return i * shape_[1] + j;
        else return (i * shape_[1] + j) * shape_[2] + k;
    }

    std::array<std::size_t, ND> shape_;
    std::vector<T> values_;
};

// Matplotlib's helper: the first dimension, or 0 when any dimension is empty.
template <typename T, int ND>
std::size_t safe_first_shape(const NdArray<T, ND>& a) {
    for (int i = 0; i < ND; ++i)
        if (a.shape(i) == 0) return 0;
    return a.shape(0);
}

}  // namespace mpl
