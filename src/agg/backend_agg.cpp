// Derived from Matplotlib 3.11.2: src/_backend_agg.cpp.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

/* -*- mode: c++; c-basic-offset: 4 -*- */

#include <cstdint>
#include "backend_agg.h"

// The most blocks of cells (4096 cells each) the rasterizer takes for one path before it gives up
// with "Exceeded cell block limit": matplotlib's 32768, some 2 GB of cells. A 32-bit target
// (WebAssembly) has no room for that and would die of a failed allocation: there the limit is 4096
// blocks (256 MB), so a path of more than 16.7 million cells is refused where matplotlib still draws
// it (up to 134 million).
#if UINTPTR_MAX > 0xffffffffu
static const unsigned kCellBlockLimit = 32768;
#else
static const unsigned kCellBlockLimit = 4096;
#endif

RendererAgg::RendererAgg(unsigned int width, unsigned int height, double dpi)
    : width(width),
      height(height),
      dpi(dpi),
      NUMBYTES((size_t)width * (size_t)height * 4),
      pixBuffer(nullptr),
      renderingBuffer(),
      alphaBuffer(nullptr),
      alphaMaskRenderingBuffer(),
      alphaMask(alphaMaskRenderingBuffer),
      pixfmtAlphaMask(alphaMaskRenderingBuffer),
      rendererBaseAlphaMask(),
      rendererAlphaMask(),
      scanlineAlphaMask(),
      slineP8(),
      slineBin(),
      pixFmt(),
      rendererBase(),
      rendererAA(),
      rendererBin(),
      theRasterizer(kCellBlockLimit),
      lastclippath(nullptr),
      _fill_color(agg::rgba(1, 1, 1, 0))
{
    if (dpi <= 0.0) {
        error = "dpi must be positive";
    }

    if (width >= 1 << 23 || height >= 1 << 23) {
        error = "Image size of " + std::to_string(width) + "x" + std::to_string(height) +
            " pixels is too large. It must be less than 2^23 in each direction.";
    }

    if (!error.empty()) {
        // No buffers: every drawing call would be meaningless; the caller must check error.
        this->width = this->height = 0;
        hatch_size = 0;
        hatchBuffer = nullptr;
        return;
    }

    unsigned stride(width * 4);

    pixBuffer = new agg::int8u[NUMBYTES];
    renderingBuffer.attach(pixBuffer, width, height, stride);
    pixFmt.attach(renderingBuffer);
    rendererBase.attach(pixFmt);
    rendererBase.clear(_fill_color);
    rendererAA.attach(rendererBase);
    rendererBin.attach(rendererBase);
    hatch_size = int(dpi);
    hatchBuffer = new agg::int8u[hatch_size * hatch_size * 4];
    hatchRenderingBuffer.attach(hatchBuffer, hatch_size, hatch_size, hatch_size * 4);
}

RendererAgg::~RendererAgg()
{
    delete[] hatchBuffer;
    delete[] alphaBuffer;
    delete[] pixBuffer;
}

void RendererAgg::create_alpha_buffers()
{
    if (!alphaBuffer) {
        alphaBuffer = new agg::int8u[width * height];
        alphaMaskRenderingBuffer.attach(alphaBuffer, width, height, width);
        rendererBaseAlphaMask.attach(pixfmtAlphaMask);
        rendererAlphaMask.attach(rendererBaseAlphaMask);
    }
}

BufferRegion *RendererAgg::copy_from_bbox(agg::rect_d in_rect)
{
    agg::rect_i rect(
        (int)in_rect.x1, height - (int)in_rect.y2, (int)in_rect.x2, height - (int)in_rect.y1);

    BufferRegion *reg = nullptr;
    reg = new BufferRegion(rect);

    agg::rendering_buffer rbuf;
    rbuf.attach(reg->get_data(), reg->get_width(), reg->get_height(), reg->get_stride());

    pixfmt pf(rbuf);
    renderer_base rb(pf);
    rb.copy_from(renderingBuffer, &rect, -rect.x1, -rect.y1);

    return reg;
}

void RendererAgg::restore_region(BufferRegion &region)
{
    if (region.get_data() == nullptr) {
        error = "Cannot restore_region from NULL data";
        return;
    }

    agg::rendering_buffer rbuf;
    rbuf.attach(region.get_data(), region.get_width(), region.get_height(), region.get_stride());

    rendererBase.copy_from(rbuf, nullptr, region.get_rect().x1, region.get_rect().y1);
}

// Restore the part of the saved region with offsets
void
RendererAgg::restore_region(BufferRegion &region, int xx1, int yy1, int xx2, int yy2, int x, int y )
{
    if (region.get_data() == nullptr) {
        error = "Cannot restore_region from NULL data";
        return;
    }

    agg::rect_i &rrect = region.get_rect();

    agg::rect_i rect(xx1 - rrect.x1, (yy1 - rrect.y1), xx2 - rrect.x1, (yy2 - rrect.y1));

    agg::rendering_buffer rbuf;
    rbuf.attach(region.get_data(), region.get_width(), region.get_height(), region.get_stride());

    rendererBase.copy_from(rbuf, &rect, x, y);
}

bool RendererAgg::render_clippath(mpl::PathIterator &clippath,
                                  const agg::trans_affine &clippath_trans,
                                  e_snap_mode snap_mode)
{
    bool has_clippath = (clippath.total_vertices() != 0);

    if (has_clippath &&
        (clippath.get_id() != lastclippath || clippath_trans != lastclippath_transform)) {
        create_alpha_buffers();
        agg::trans_affine trans(clippath_trans);
        trans *= agg::trans_affine_scaling(1.0, -1.0);
        trans *= agg::trans_affine_translation(0.0, (double)height);

        rendererBaseAlphaMask.clear(agg::gray8(0, 0));
        auto transformed_clippath = agg::conv_transform{clippath, trans};
        auto nan_removed_clippath = PathNanRemover{
            transformed_clippath, true, clippath.has_codes()};
        // Unlike normal Paths, the clip path cannot be clipped to the Figure
        // bbox, because it needs to remain a complete closed path, so there is
        // no PathClipper step after nan-removal.
        auto snapped_clippath = PathSnapper{
            nan_removed_clippath, snap_mode, clippath.total_vertices(), 0.0};
        auto simplified_clippath = PathSimplifier{
            snapped_clippath,
            clippath.should_simplify() && !clippath.has_codes(),
            clippath.simplify_threshold()};
        auto curved_clippath = agg::conv_curve{simplified_clippath};
        theRasterizer.add_path(curved_clippath);
        rendererAlphaMask.color(agg::gray8(255, 255));
        agg::render_scanlines(theRasterizer, scanlineAlphaMask, rendererAlphaMask);
        lastclippath = clippath.get_id();
        lastclippath_transform = clippath_trans;
    }

    return has_clippath;
}

void RendererAgg::clear()
{
    //"clear the rendered buffer";

    rendererBase.clear(_fill_color);
}
