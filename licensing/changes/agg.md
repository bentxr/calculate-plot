# Changes to Anti-Grain Geometry 2.4 (as vendored by Matplotlib 3.11.2)

The files in extern/agg24-svn are Matplotlib 3.11.2's copy of Agg 2.4 (extern/agg24-svn there),
which Matplotlib itself patched. calculate-plot copies them unmodified, except:

- include/agg_rasterizer_cells_aa.h: `add_curr_cell()` calls `AGG_CELL_BLOCK_LIMIT_HANDLER()` and returns
  when that macro is defined, instead of throwing `std::overflow_error`, so that the library can be built
  without C++ exceptions (src/agg/agg_hooks.h defines the macro; the renderer reports the error). Without the
  macro the file behaves exactly as Matplotlib's. For the same reason `line()` returns at once, and it and
  `render_hline()` leave their loops over scanlines and cells, when `AGG_CELL_BLOCK_LIMIT_REACHED()` (also from agg_hooks.h) says that the limit has
  been hit: Matplotlib's copy has left the function by throwing at that point; without the check a path far
  larger than the canvas kept the rasterizer walking its scanlines for minutes with nowhere to put the cells.
  `sort_cells()` then keeps no cell of such a path, so that nothing of it is swept or drawn.
- include/agg_rasterizer_scanline_aa.h: for the same reason `add_path()` leaves its loop over the vertices of
  a path when `AGG_CELL_BLOCK_LIMIT_REACHED()` says the cells are full, as the exception does upstream (the
  curves that follow are then not flattened for nothing).
- include/agg_conv_gpc.h is not copied: it adapts the General Polygon Clipper, which is not free for
  commercial use, and nothing uses it.
- Only the headers and the eight source files Matplotlib's build uses are copied.
