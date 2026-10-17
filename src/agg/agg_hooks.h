#pragma once

// How the vendored Agg reports that a path needs more rasterizer cells than its limit: it calls
// this hook (see the change marked in extern/agg24-svn/include/agg_rasterizer_cells_aa.h) where
// matplotlib's copy throws. Include this header before any Agg header.

namespace calculate_plot::agg_hooks {
bool& cellBlockLimitExceeded();  // per thread; the renderer clears and checks it around each call
}  // namespace calculate_plot::agg_hooks

#define AGG_CELL_BLOCK_LIMIT_HANDLER() (calculate_plot::agg_hooks::cellBlockLimitExceeded() = true)
// Asked only once the last block of cells is in use: whether the limit has been hit, so that the
// rasterizer can stop where matplotlib's has already thrown, and not walk on over every scanline of
// a path far larger than the canvas.
#define AGG_CELL_BLOCK_LIMIT_REACHED() (calculate_plot::agg_hooks::cellBlockLimitExceeded())
