// Derived from ContourPy 1.4.0: src/contour_generator.h.
// Copyright (c) 2021-2026, ContourPy Developers. All rights reserved.
// Licence: LICENSES/contourpy.txt. Changes: licensing/changes/contourpy.md
#ifndef CONTOURPY_CONTOUR_GENERATOR_H
#define CONTOURPY_CONTOUR_GENERATOR_H

#include "common.h"

namespace contourpy {

class ContourGenerator
{
public:
    // Non-copyable and non-moveable.
    ContourGenerator(const ContourGenerator& other) = delete;
    ContourGenerator(const ContourGenerator&& other) = delete;
    ContourGenerator& operator=(const ContourGenerator& other) = delete;
    ContourGenerator& operator=(const ContourGenerator&& other) = delete;

    virtual ~ContourGenerator() = default;

    virtual py::tuple filled(double lower_level, double upper_level) = 0;

    virtual const char* get_name() const = 0;

    virtual py::sequence lines(double level) = 0;

protected:
    ContourGenerator() = default;

    // (the caller has checked the levels: neither is NaN, and lower is less than upper)
    void check_levels(double, double) const {}
};

} // namespace contourpy

#endif // CONTOURPY_CONTOUR_GENERATOR_H
