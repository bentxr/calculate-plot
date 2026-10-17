// Derived from Matplotlib 3.11.2: src/py_adaptors.h.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#ifndef MPL_PY_ADAPTORS_H
#define MPL_PY_ADAPTORS_H

/***************************************************************************
 * The Path adaptors of matplotlib's renderer, holding their data in C++ vectors instead of NumPy
 * arrays. Vertices are shared between copies, as NumPy arrays are shared between references.
 */

#include <cstdint>
#include <memory>
#include <vector>

#include "agg_basics.h"

namespace mpl {

/************************************************************
 * mpl::PathIterator acts as a bridge between the path data and Agg: it iterates over the
 * vertices and codes using the standard Agg vertex source interface:
 *
 *     unsigned vertex(double* x, double* y)
 */
class PathIterator
{
    std::shared_ptr<const std::vector<double>> m_vertices;  // x0, y0, x1, y1, ...
    std::shared_ptr<const std::vector<uint8_t>> m_codes;    // empty pointer: no codes

    unsigned m_iterator;
    unsigned m_total_vertices;

    /* This class doesn't actually do any simplification, but we
       store the value here, since it is obtained from the Path. */
    bool m_should_simplify;
    double m_simplify_threshold;

    // What matplotlib uses to recognise the same clip path again: there, the address of the
    // Python vertex array. Paths built from a trace carry the identity the trace recorded.
    const void *m_id;

  public:
    inline PathIterator()
        : m_iterator(0),
          m_total_vertices(0),
          m_should_simplify(false),
          m_simplify_threshold(1.0 / 9.0),
          m_id(nullptr)
    {
    }

    inline PathIterator(std::shared_ptr<const std::vector<double>> vertices,
                        std::shared_ptr<const std::vector<uint8_t>> codes,
                        bool should_simplify, double simplify_threshold, const void *id = nullptr)
        : m_iterator(0)
    {
        set(std::move(vertices), std::move(codes), should_simplify, simplify_threshold, id);
    }

    inline PathIterator(const PathIterator &other)
    {
        m_vertices = other.m_vertices;
        m_codes = other.m_codes;

        m_iterator = 0;
        m_total_vertices = other.m_total_vertices;

        m_should_simplify = other.m_should_simplify;
        m_simplify_threshold = other.m_simplify_threshold;
        m_id = other.m_id;
    }

    inline PathIterator &operator=(const PathIterator &other)
    {
        m_vertices = other.m_vertices;
        m_codes = other.m_codes;
        m_iterator = 0;
        m_total_vertices = other.m_total_vertices;
        m_should_simplify = other.m_should_simplify;
        m_simplify_threshold = other.m_simplify_threshold;
        m_id = other.m_id;
        return *this;
    }

    inline void
    set(std::shared_ptr<const std::vector<double>> vertices,
        std::shared_ptr<const std::vector<uint8_t>> codes,
        bool should_simplify, double simplify_threshold, const void *id = nullptr)
    {
        m_should_simplify = should_simplify;
        m_simplify_threshold = simplify_threshold;

        m_vertices = std::move(vertices);
        m_total_vertices = m_vertices ? unsigned(m_vertices->size() / 2) : 0;
        m_codes = std::move(codes);
        m_id = id ? id : static_cast<const void *>(m_vertices.get());

        m_iterator = 0;
    }

    inline unsigned vertex(double *x, double *y)
    {
        if (m_iterator >= m_total_vertices) {
            *x = 0.0;
            *y = 0.0;
            return agg::path_cmd_stop;
        }

        const size_t idx = m_iterator++;

        *x = (*m_vertices)[2 * idx];
        *y = (*m_vertices)[2 * idx + 1];

        if (m_codes) {
            return (*m_codes)[idx];
        } else {
            return idx == 0 ? agg::path_cmd_move_to : agg::path_cmd_line_to;
        }
    }

    inline void rewind(unsigned path_id)
    {
        m_iterator = path_id;
    }

    inline unsigned total_vertices() const
    {
        return m_total_vertices;
    }

    inline bool should_simplify() const
    {
        return m_should_simplify;
    }

    inline double simplify_threshold() const
    {
        return m_simplify_threshold;
    }

    inline bool has_codes() const
    {
        return bool(m_codes);
    }

    inline void *get_id() const
    {
        return const_cast<void *>(m_id);  // compared, never written through
    }
};

class PathGenerator
{
    std::vector<PathIterator> m_paths;

  public:
    typedef PathIterator path_iterator;

    PathGenerator() = default;
    explicit PathGenerator(std::vector<PathIterator> paths) : m_paths(std::move(paths)) {}

    size_t num_paths() const
    {
        return m_paths.size();
    }

    size_t size() const
    {
        return m_paths.size();
    }

    path_iterator operator()(size_t i)
    {
        return m_paths[i % m_paths.size()];
    }
};
}

#endif
