# Summary of changes to Matplotlib (clause 3 of the Matplotlib License)

calculate-plot contains C++ code derived from Matplotlib 3.11.2 (https://matplotlib.org,
https://github.com/matplotlib/matplotlib/tree/v3.11.2). Each derived file names the Matplotlib file
it comes from in its first lines. The changes, as a whole:

- **Python bindings removed.** pybind11 and the Python C API are gone. NumPy array views are
  replaced by `mpl::NdArray` (src/agg/ndarray.h, our own) with the same interface, and Path objects
  by `mpl::PathIterator`/`mpl::PathGenerator` holding C++ vectors (src/agg/path.h, from
  src/py_adaptors.h). The pybind11 type casters' conversion rules (graphics context, dashes, colours,
  affines, bounding boxes, snapping) are carried over into src/render/replay.cpp, which reads them from
  a recorded trace instead of Python objects.
- **No exceptions.** Where Matplotlib throws (renderer construction, restore_region, draw_gouraud_triangles
  argument checks, Agg's cell block limit, check_trailing_shape), the port records the reason in
  `RendererAgg::error` (or returns false) and the caller reports it. draw_markers' try/catch, which only
  restored the clipping state before rethrowing, is now a plain block (its normal path already restores it).
- **Clip path identity** (`PathIterator::get_id`) is an explicit identity carried by the path (recorded from
  Matplotlib) instead of the address of a Python array.
- **Text** (src/text): src/ft2font.cpp and .h without pybind11; fonts are opened from a file path with
  FT_New_Face instead of a Python file stream; the module-level FreeType library is created on first use;
  missing-glyph warnings are collected instead of issued through Python; the FT2Image whole-string bitmap
  path (draw_glyphs_to_bitmap, draw_glyph_to_bitmap) is not included. From src/ft2font_wrapper.cpp:
  FT2Font._layout (layout items) and FT2Font._render_glyph (with the bitmap conversion). From Python,
  translated to C++: backend_agg.py's get_hinting_flag, draw_text (plain text), _prepare_font and
  _draw_text_glyphs_and_boxes, and font_manager.py's _get_font (fallback chain and Last Resort font).
  Python's and NumPy's round() are std::nearbyint (halves to even). Fonts are named by file name in a
  font directory. When a text has no language, the port passes the reference locale's language to
  libraqm explicitly instead of letting HarfBuzz read the process locale.
- **SVG** (src/svg, src/agg/path_text.h): backend_svg.py's RendererSVG and _XMLWriter (from Fredrik
  Lundh's SimpleXMLWriter, whose notice is kept), backend_bases.py's path collection helpers, textpath.py's
  glyph handling and colors.to_hex, translated to C++; _path.h's cleanup_path and convert_to_string with
  Python's float formatting replaced by locale-independent std::to_chars. Python's str()/repr() of the values
  matplotlib hashes into element ids are reproduced. Embedded images are PNGs from calculate-plot's own
  encoder (matplotlib uses Pillow). The metadata never names Matplotlib as the creator and never writes the
  current time: a Date must be given (or None).
- **Ticks** (src/axes/ticker): ticker.py's scale_range, _Edge_integer, MaxNLocator (AutoLocator) and
  ScalarFormatter (offset, order of magnitude, format, labels and offset text, with the default rcParams) and
  transforms.py's _nonsingular, translated to C++ with Python's and NumPy's numeric semantics (floor division,
  divmod, rounding, %-formatting) reproduced exactly; the axis' view interval is passed in.
- **Log ticks** (src/axes/log_ticker): ticker.py's LogLocator (tick_values, nonsingular, view_limits, the decade
  helpers), LogFormatter.set_locs and LogFormatterMathtext/LogFormatterSciNotation.__call__, and the overlap removal
  of Axis.get_minorticklocs, translated to C++ (the locator's axis is given as its tick space and minpos).
- **Transforms** (src/transforms): transforms.py's Bbox, TransformedBbox, Affine2D, IdentityTransform,
  BboxTransformTo/From, BboxTransform, BlendedAffine2D, ScaledTranslation and the composite transforms, translated
  to C++ (matrices computed on demand rather than cached and invalidated; the same values). np.dot is reproduced
  (accumulating from +0.0) and numpy.linalg.inv by an LU solve that multiplies by pivot reciprocals as NumPy's
  OpenBLAS does; composites keep matplotlib's distinction between CompositeAffine2D and CompositeGenericTransform.
  Non-affine transforms are supported for log scales: LogTransform/InvertedLogTransform (one-dimensional, used
  inside blended transforms), BlendedGenericTransform, CompositeGenericTransform's non-affine part and get_affine.
- **Font manager** (src/text/font_manager, data/fonts.json): font_manager.py's FontProperties defaults, font
  size names, the scoring functions and _find_fonts_by_props/_findfont_cached, translated to C++, over a fixed list of
  the bundled fonts (dumped from matplotlib's FontManager, sorted by file name) instead of a scan of the system's
  fonts with a cache file; font files are named by file name.
- **Mathtext** (src/text/mathtext, src/text/mathtext_fonts, data/tex2uni.json, data/mathtext_data.json):
  _mathtext.py translated to C++: the box model (Node, Box, Vbox, Hbox, Char, Accent, List, Hlist with kerning and
  hpack, Vlist with vpack, Rule, Hrule, Vrule, Glue, HCentered, VCentered, Kern, AutoHeightChar, AutoWidthChar, ship),
  the fontsets (Fonts, TruetypeFonts, BakomaFonts, UnicodeFonts, DejaVuFonts, DejaVuSerifFonts, DejaVuSansFonts,
  StixFonts, StixSansFonts and the font constants), the Parser's handlers, and mathtext.py's vector output. The
  pyparsing grammar is replaced by a hand-written recursive-descent parser that accepts what the grammar accepts
  and skips whitespace where pyparsing does; its error messages are not pyparsing's. The tables of
  _mathtext_data.py (tex2uni, latex_to_bakoma, stix_virtual_fonts, stix_glyph_fixes) and the symbol sets of the
  Parser class are data files written by tools/oracle/make_tex2uni.py and tools/oracle/make_mathtext_data.py.
  Letters, Greek capitals and digits are told with data/unicode_letters.json instead of Python's unicodedata.
  The raster output (Output.to_raster) and the font name cache of the fontsets are not ported. A fontset whose
  fonts cannot be had (a file that does not open, a pattern FontProperties refuses) fails with an error. Where
  matplotlib itself breaks on an expression (a quoted string or braces as a row of \substack, a delimiter left
  unsized) the expression is refused.
- **Fontconfig patterns** (src/support/fontconfig_pattern, the pattern part of src/text/font_manager):
  _fontconfig_pattern.py's parse_fontconfig_pattern and FontProperties' set_fontconfig_pattern with its setters,
  translated to C++; the pyparsing grammar is a hand-written parser that expands tabs and skips blanks as
  pyparsing does. A pattern that names a font file (file=) is refused where a font is asked for with it.
- **The validators of the rcParams** (src/rc/rc_validate, and "validators" in data/rcdefaults.json): rcsetup.py's
  validators (ValidateInStrings, _listify_validator, validate_bool, validate_color and the others) and colors.py's
  is_color_like, translated to C++ over the values of a figure file (JSON) instead of Python objects; which key has
  which validator is a table written by tools/oracle/make_rcdefaults.py. The stored values are JSON values (a tuple
  is a list). Text that matplotlib reads with ast.literal_eval is read by calculate-plot's own reader of Python
  literals (src/support/pyliteral), which leaves some spellings unread; cyclers written as text, dates and backend
  names are not validated but refused as not supported. The error messages are calculate-plot's.
  path.py's Path (vertices, codes and the simplification settings),
  backend_bases.py's GraphicsContextBase and the RendererBase interface, artist.py's Artist (clip settings),
  patches.py's Patch and Rectangle, lines.py's dash patterns and figure.py's Figure (its boxes, transforms and
  patch) with backend_agg.py's and backend_svg.py's canvas drawing, translated to C++. Artists draw through a
  renderer interface; calculate-plot's renderers record the calls in its trace format instead of drawing
  directly. Colours are given as strings for now. text.py's Text (_get_layout, draw, get_window_extent, the
  rotation and math-text checks) and textpath.py's TextToPath.get_text_width_height_descent are translated to C++;
  the font's line metrics are read with FreeType's FT_Get_Sfnt_Table. gridspec.py's SubplotParams, GridSpec
  (get_grid_positions) and SubplotSpec.get_position, translated to C++ (NumPy's cumsum and Python's sum()
  reproduced; a grid's own spacing overrides the figure's subplot parameters as in get_subplot_params).
  lines.py's Line2D (lines and markers; drawing through affine transforms) and markers.py's MarkerStyle (the tick
  and line markers so far; integer markers go by name), axis.py's Tick/XTick/YTick and Axis/XAxis/YAxis (major
  ticks from AutoLocator and ScalarFormatter, tick visibility from the rcParams), spines.py's linear Spine and
  axes/_base.py's _AxesBase (position from the subplot spec, transforms, patch, spines, axes, titles, drawing
  order) are translated to C++; ticks are made when the axis is drawn rather than lazily on first access, with the
  same visibility. transforms.py's TransformWrapper is added to the transforms. Also translated: Axes.set_title,
  set_xlabel/set_ylabel, set_xlim/set_ylim (Axis._set_lim), the title placement (_update_title_position, for an Axes
  without twins or top ticks), XAxis/YAxis._update_label_position and _update_offset_text_position, Spine's window
  extent with its ticks, Bbox.union, and XAxis/YAxis.get_tick_space feeding AutoLocator's 'auto' bins. For
  Axes.plot: _process_plot_format, the argument grouping of _process_plot_var_args, the property cycle's colours,
  add_line (clipping to the Axes patch, data limits as Bbox.update_from_path), autoscale_view (linear scales, no
  sticky edges yet) with MaxNLocator.view_limits, and every marker of markers.py with full or no fill (half fills
  come later); the unit polygons and star the markers use are data (data/marker_paths.json) taken from
  matplotlib, since NumPy's vectorised sin and cos made them. Log scales: scale.py's LogScale (locators,
  formatters, limit_range_for_scale), Axis._set_axes_scale (with its immediate autoscale), minor ticks
  (Axis._update_ticks, get_minorticklocs), the data limits' minpos, autoscale_view through the scale's transform, and
  Axes.viewLim running a pending autoscale before it is read, translated to C++; mathtext texts are drawn and
  measured through the mathtext layout in both renderers. collections.py's Collection (colours with 'none' and
  'face', line widths, antialiasing, the draw with its single-path draw_markers optimisation and the hatch-colour
  probe call of draw_path_collection, the data limits of offset collections) and PathCollection (sizes), and
  axes/_axes.py's scatter with _parse_scatter_color_args and Axes.add_collection, translated to C++; the patches'
  own colour cycle is kept apart from the lines'. colors.py's ListedColormap (with its under/over/bad entries),
  Normalize and cm.py's ScalarMappable.to_rgba map scatter's values (data/colormaps.json holds viridis so far), with
  Collection.update_scalarmappable's choice of mapped faces and edges. PolyCollection (closed polygons) and
  FillBetweenPolyCollection (_make_verts, _make_verts_for_region without steps or interpolation yet, the data
  box), with Axes.fill_between's arguments and the collections' general paths transform. LineCollection (its
  defaults from lines.*), the vlines/hlines insertion (add_collection without autolimits, then the corners), and
  Axes.errorbar (data line, error bars, caps; no limit markers or errorevery yet), translated to C++.
  Legends: legend.py's Legend (options from the rcParams, the frame, _init_legend_box, the anchored and 'best'
  locations, draw), offsetbox.py's TextArea, DrawingArea, HPacker, VPacker with _get_packed_offsets and
  _get_aligned_offsets, legend_handler.py's HandlerLine2D, HandlerPathCollection and HandlerPolyCollection,
  patches.py's FancyBboxPatch with the round and square box styles, Line2D.update_from and markevery lists,
  Collection.update_from, and src/_path.h's path_intersects_rectangle (unfilled) and count_bboxes_overlapping_bbox
  (in src/agg/path_queries.h), translated to C++. Shadows, other handlers and figure legends come later.
  Spans and grids: Axes.axhline/axvline, axhspan/axvspan, vlines/hlines and grid with Axis.grid's keywords on the
  ticks; Axes.add_patch with _update_patch_limits, the general _update_line_limits (lines in axes coordinates, via
  transform - transData), Bbox.update_from_path's per-direction updates, contains_branch_separately, and the
  collections' line styles (_bcast_lwls), translated to C++. Figure.subplots grids (the returned array's names
  follow squeeze) and FigureBase.suptitle (_suplabels), translated to C++.
  Layout: _tight_layout.py's _auto_adjust_subplotpars, get_subplotspec_list and get_tight_layout_figure,
  TightLayoutEngine.execute, Figure.tight_layout and FigureBase.subplots_adjust, with the boxes they measure
  (Axes.get_tightbbox and get_default_bbox_extra_artists, Axis.get_tightbbox, Legend.get_tightbbox), translated
  to C++; Spine.get_window_extent now carries the path through the whole transform and counts a major and a minor
  tick, as matplotlib does. The children clipped by the Axes' patch are not measured: their boxes, cut by the clip
  box, lie inside the patch's own box, which is counted. Warnings are not reproduced (the layout is left alone
  where matplotlib warns that it is not applied).
  Shared axes: GridSpecBase.subplots' sharex and sharey, Axes.sharex/sharey as plt.subplots uses them (cbook.Grouper's
  order of the siblings is kept in a vector; the shared Ticker objects become the Axis the locators belong to),
  the siblings in Axis._set_lim, _unstale_viewLim, autoscale_view, Axis._set_axes_scale and LogLocator.nonsingular,
  _label_outer_xaxis/_label_outer_yaxis and what Axis.set_tick_params does to the offset text, translated to C++.
  Collection._prepare_points: scatter's offsets are a masked array, so the logarithm of a non-positive offset is
  NaN, not the clipped -1000.
  Saving: Figure.savefig (transparent), FigureCanvasBase.print_figure (rcParams savefig.dpi, facecolor, edgecolor,
  bbox 'tight' with pad_inches), FigureBase.get_tightbbox, _tight_bbox.adjust_bbox with its restore, and
  FigureCanvasBase.get_width_height (the canvas in whole pixels), translated to C++. The draw with drawing
  disabled that print_figure makes before measuring is left out: get_tightbbox places what it measures.
  Transforms: equality (AffineBase.__eq__ by matrix, _BlendedMixin.__eq__, CompositeGenericTransform.__eq__,
  TransformWrapper.__eq__, with Python's order of trying both sides), depth, _iter_break_from_left_to_right,
  contains_branch, contains_branch_separately, Transform.__sub__ and BlendedGenericTransform.inverted, translated
  to C++; Axes._update_line_limits and _update_patch_limits use them. np.dot is a fused sum (see the comment in
  src/transforms/transforms.cpp).
  Titles: _update_title_position now clears the y axis' offset text and the x axis' top ticks and labels
  (Axis._get_ticks_position over a major and a minor tick), as matplotlib does.
  Texts: axes/_axes.py's Axes.text with text.py's Text.__init__ and Text.update (the property setters of Text,
  FontProperties and Artist, each with its check), translated to C++ in the executor; backend_agg.py's
  _draw_text_glyphs_and_boxes now also records the boxes of mathtext (rules) and their filled paths.
- File names drop the leading underscore (e.g. src/_backend_agg.h is src/agg/backend_agg.h); includes follow.
- RendererAgg's rasterizer takes Matplotlib's 32 768 blocks of cells on 64-bit targets and 4 096 on 32-bit ones
  (WebAssembly), which have no memory for more: a path that needs more is refused there with the same
  "Exceeded cell block limit".
- The drawing algorithms themselves (src/_backend_agg.h, src/_backend_agg.cpp, src/path_converters.h,
  src/agg_workaround.h, src/array.h, src/mplutils.h) are otherwise unchanged.
