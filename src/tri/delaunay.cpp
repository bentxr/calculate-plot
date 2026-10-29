// Derived from Matplotlib 3.11.2: src/_qhull_wrapper.cpp (delaunay, delaunay_impl, at_least_3_unique_points,
// get_facet_vertices, get_facet_neighbours).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "tri/delaunay.hpp"

#include "libqhull_r/qhull_ra.h"

#include <cstdio>

namespace calculate_plot::tri {

namespace {

const char* const kQhullErrors[6] = {"",
                                     "input inconsistency",   // qh_ERRinput
                                     "singular input data",   // qh_ERRsingular
                                     "precision error",       // qh_ERRprec
                                     "insufficient memory",   // qh_ERRmem
                                     "internal error"};       // qh_ERRqhull

// the 3 point indices of a facet (a triangle after qh_triangulate)
void facetVertices(qhT* qh, const facetT* facet, int indices[3]) {
    vertexT *vertex, **vertexp;
    FOREACHvertex_(facet->vertices) { *indices++ = qh_pointid(qh, vertex->point); }
}

// the 3 triangles next to a facet (-1 for the upper Delaunay ones)
void facetNeighbours(const facetT* facet, const std::vector<int>& triIndices, int indices[3]) {
    facetT *neighbor, **neighborp;
    FOREACHneighbor_(facet) { *indices++ = neighbor->upperdelaunay ? -1 : triIndices[std::size_t(neighbor->id)]; }
}

bool atLeast3UniquePoints(std::size_t n, const double* x, const double* y) {
    const std::size_t unique1 = 0;
    std::size_t unique2 = 0;
    if (n < 3) return false;
    for (std::size_t i = 1; i < n; ++i) {
        if (unique2 == 0) {
            if (x[i] != x[unique1] || y[i] != y[unique1]) unique2 = i;
        } else if ((x[i] != x[unique1] || y[i] != y[unique1]) && (x[i] != x[unique2] || y[i] != y[unique2])) {
            return true;
        }
    }
    return false;
}

// QhullInfo: what Qhull holds is freed and its error file closed on every way out
class QhullInfo {
public:
    QhullInfo(FILE* errorFile, qhT* qh) : errorFile_(errorFile), qh_(qh) {}
    ~QhullInfo() {
        qh_freeqhull(qh_, !qh_ALL);
        int curlong = 0, totlong = 0;
        qh_memfreeshort(qh_, &curlong, &totlong);
        std::fclose(errorFile_);
    }
    QhullInfo(const QhullInfo&) = delete;
    QhullInfo& operator=(const QhullInfo&) = delete;

private:
    FILE* errorFile_;
    qhT* qh_;
};

}  // namespace

bool delaunay(const std::vector<double>& x, const std::vector<double>& y, Delaunay* out, std::string* error) {
    const std::size_t npoints = x.size();
    if (npoints != y.size()) return *error = "x and y must be 1D arrays of the same length", false;
    if (npoints < 3) return *error = "x and y arrays must have a length of at least 3", false;
    if (!atLeast3UniquePoints(npoints, x.data(), y.data()))
        return *error = "x and y arrays must consist of at least 3 unique points", false;

    qhT qh_qh;
    qhT* qh = &qh_qh;
    facetT* facet;
    const int ndim = 2;
    QHULL_LIB_CHECK
    // the points less their mean
    double xMean = 0.0, yMean = 0.0;
    for (std::size_t i = 0; i < npoints; ++i) {
        xMean += x[i];
        yMean += y[i];
    }
    xMean /= double(npoints);
    yMean /= double(npoints);
    std::vector<coordT> points(npoints * ndim);
    for (std::size_t i = 0; i < npoints; ++i) {
        points[2 * i] = x[i] - xMean;
        points[2 * i + 1] = y[i] - yMean;
    }
    // Qhull's messages go nowhere (matplotlib's unless Python runs verbose)
    FILE* errorFile = std::fopen("/dev/null", "w");
    if (!errorFile) return *error = "Could not open devnull", false;
    QhullInfo info(errorFile, qh);
    qh_zero(qh, errorFile);
    char options[] = "qhull d Qt Qbb Qc Qz";
    const int exitcode = qh_new_qhull(qh, ndim, int(npoints), points.data(), False, options, nullptr, errorFile);
    if (exitcode != qh_ERRnone) {
        const int code = exitcode >= 0 && exitcode < 6 ? exitcode : 5;
        return *error = std::string("Error in qhull Delaunay triangulation calculation: ") + kQhullErrors[code] + " (exitcode=" +
                        std::to_string(exitcode) + "); use python verbose option (-v) to see original qhull error.",
               false;
    }
    qh_triangulate(qh);  // facets of 3 points each
    std::vector<int> triIndices(std::size_t(qh->facet_id));
    out->triangles.clear();
    out->neighbors.clear();
    int i = 0;
    FORALLfacets {
        if (!facet->upperdelaunay) {
            int indices[3] = {0, 0, 0};
            triIndices[std::size_t(facet->id)] = i++;
            facetVertices(qh, facet, indices);
            out->triangles.push_back({facet->toporient ? indices[0] : indices[2], indices[1], facet->toporient ? indices[2] : indices[0]});
        } else {
            triIndices[std::size_t(facet->id)] = -1;
        }
    }
    FORALLfacets {
        if (!facet->upperdelaunay) {
            int indices[3] = {0, 0, 0};
            facetNeighbours(facet, triIndices, indices);
            out->neighbors.push_back({facet->toporient ? indices[2] : indices[0], facet->toporient ? indices[0] : indices[2], indices[1]});
        }
    }
    return true;
}

}  // namespace calculate_plot::tri
