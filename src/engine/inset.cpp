// Derived from Matplotlib 3.11.2: lib/matplotlib/inset.py (InsetIndicator) and lib/matplotlib/patches.py
// (ConnectionPatch).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/inset.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "support/raised.hpp"

namespace calculate_plot::engine {

InsetIndicator::InsetIndicator(const rc::Params& rc, Axes& parent, Axes* inset, std::optional<std::array<double, 4>> bounds)
    : Artist(rc), rectangle(rc, 0, 0, 1, 1), parent_(parent), inset_(inset), autoBounds_(!bounds) {
    zorder = 4.99;
    if (bounds) {
        rectangle.x0 = (*bounds)[0], rectangle.y0 = (*bounds)[1], rectangle.width = (*bounds)[2], rectangle.height = (*bounds)[3];
    }
    rectangle.clipOn = false;
}

void InsetIndicator::draw(Renderer& renderer) {
    if (!visible) return;
    if (!inset_) {  // no connectors: the rectangle alone
        rectangle.draw(renderer);
        return;
    }
    // connectors: the rectangle follows the inset's limits, then the four corners are joined
    if (autoBounds_) {
        const auto xl = inset_->xaxis->viewInterval(), yl = inset_->yaxis->viewInterval();
        rectangle.x0 = xl[0], rectangle.y0 = yl[0], rectangle.width = xl[1] - xl[0], rectangle.height = yl[1] - yl[0];
    }
    const double x = rectangle.x0, y = rectangle.y0, w = rectangle.width, h = rectangle.height;
    const auto xv = parent_.xaxis->viewInterval(), yv = parent_.yaxis->viewInterval();
    const bool xInverted = xv[1] < xv[0], yInverted = yv[1] < yv[0];
    std::vector<Path> connectors;
    const double corners[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    for (const auto& corner : corners) {
        double ex = corner[0], ey = corner[1];
        if (xInverted) ex = 1 - ex;
        if (yInverted) ey = 1 - ey;
        // ConnectionPatch(xyA=corner, coordsA=inset.transAxes, xyB=data corner, coordsB=transData, arrowstyle="-")
        const auto a = inset_->transAxes->transform(corner[0], corner[1]);
        const auto b = rectangle.transform->transform(x + ex * w, y + ey * h);
        ConnectionStyle arc3;
        Path connection;
        std::string error;
        if (!arc3(a, b, 0., 0., nullptr, nullptr, rc_, &connection, &error)) return support::noteRaised(error);
        ArrowStyle line;
        ArrowStyle::make("-", {}, &line, &error);
        std::vector<Path> paths;
        std::vector<bool> fills;
        bool list = false;
        // (mutation scale 10 and the line width times the dpi correction of a draw: the connectors are not
        // drawn by themselves, so it stays 1; '-' has no heads, which they would size)
        if (!line(connection, 10., rectangle.linewidth(), 1., rc_, &paths, &fills, &list, &error)) return support::noteRaised(error);
        connectors.push_back(compoundPath(paths, rc_));
    }
    if (!connectorVisible_) {
        // which two show: from where the rectangle and the inset lie (the rectangle's data bounds through its
        // transform, as matplotlib takes them)
        const auto pos = inset_->position->points();
        const auto fig = parent_.figure.transSubfigure;
        const auto ins0 = fig->transform(pos[0], pos[1]), ins1 = fig->transform(pos[2], pos[3]);
        const auto t = rectangle.fullTransform();
        const auto ll = t->transform(x, y), ul = t->transform(x, y + h), lr = t->transform(x + w, y);
        const bool x0 = ll[0] < ins0[0], x1 = lr[0] < ins1[0], y0 = ll[1] < ins0[1], y1 = ul[1] < ins1[1];
        connectorVisible_ = std::array<bool, 4>{x0 != y0, x0 == y1, x1 == y0, x1 != y1};
    }
    // the visible connectors (in the rectangle's style) and the rectangle as one compound path
    std::vector<Path> paths;
    const auto t = rectangle.fullTransform();
    Path rect = transformPathNonAffine(rectangle.path(), *t);
    {
        const auto m = t->matrix();
        std::vector<double> v = *rect.vertices;
        for (std::size_t k = 0; k + 1 < v.size(); k += 2) {
            const auto q = transforms::apply(m, v[k], v[k + 1]);
            v[k] = q[0], v[k + 1] = q[1];
        }
        rect = Path::make(std::move(v), rect.codes ? *rect.codes : std::vector<unsigned char>{}, rc_);
    }
    paths.push_back(rect);
    for (std::size_t i = 0; i < 4; ++i)
        if ((*connectorVisible_)[i]) paths.push_back(connectors[i]);
    if (paths.size() == 1) {
        rectangle.draw(renderer);
        return;
    }
    // PathPatch(path).update_from(rectangle), with the identity transform
    PathPatch p(rc_, compoundPath(paths, rc_));
    p.updateFrom(rectangle);
    p.transform = std::make_shared<transforms::IdentityTransform>();
    p.draw(renderer);
}

}  // namespace calculate_plot::engine
