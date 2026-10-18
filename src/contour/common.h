// Derived from ContourPy 1.4.0: src/common.h.
// Copyright (c) 2021-2026, ContourPy Developers. All rights reserved.
// Licence: LICENSES/contourpy.txt. Changes: licensing/changes/contourpy.md
//
// The pybind11 types the mpl2014 algorithm uses, as plain C++ (calculate-plot calls it from C++): input arrays
// of two dimensions, output arrays of points and codes, the lists that collect them and the tuple returned.
#ifndef CONTOURPY_COMMON_H
#define CONTOURPY_COMMON_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace contourpy {

namespace py {
using ssize_t = std::ptrdiff_t;
using size_t = std::size_t;
}  // namespace py

// quad/point index type, the same as for numpy array shape/indices (== npy_intp).  Also used for
// chunks.
typedef py::ssize_t index_t;

// Count of points, lines and holes.
typedef py::size_t count_t;

// Offsets into point arrays.
typedef uint32_t offset_t;

// A C-contiguous input array of two dimensions (ndim 0: not given), as the generator reads it.
template <typename T>
class InputArray {
public:
    InputArray() = default;
    InputArray(const T* data, index_t ny, index_t nx) : data_(data), ndim_(2), shape_{ny, nx} {}
    const T* data() const { return data_; }
    int ndim() const { return ndim_; }
    index_t shape(int i) const { return shape_[i]; }

private:
    const T* data_ = nullptr;
    int ndim_ = 0;
    index_t shape_[2] = {0, 0};
};

// Input numpy array classes.
typedef InputArray<double> CoordinateArray;
typedef InputArray<bool> MaskArray;

// An output array: its values, filled through mutable_data().
template <typename T>
class OutputArray {
public:
    explicit OutputArray(const py::ssize_t* dims, int ndim = 0) {
        std::size_t n = 1;
        for (int i = 0; i < (ndim ? ndim : (sizeof(T) == 1 ? 1 : 2)); ++i) n *= std::size_t(dims[i]);
        values.resize(n);
    }
    T* mutable_data() { return values.data(); }
    std::vector<T> values;
};

// Output numpy array classes.
typedef OutputArray<double> PointArray;
typedef OutputArray<uint8_t> CodeArray;

namespace py {
// A list of point arrays or of code arrays, in order.
struct list {
    void append(PointArray a) { points.push_back(std::move(a.values)); }
    void append(CodeArray a) { codes.push_back(std::move(a.values)); }
    std::vector<std::vector<double>> points;
    std::vector<std::vector<uint8_t>> codes;
};
// (vertices, codes): the lists of a contour level
struct tuple {
    std::vector<std::vector<double>> points;
    std::vector<std::vector<uint8_t>> codes;
};
using sequence = tuple;
inline tuple make_tuple(const list& vertices, const list& codes) { return {vertices.points, codes.codes}; }
}  // namespace py

} // namespace contourpy

#endif // CONTOURPY_COMMON_H
