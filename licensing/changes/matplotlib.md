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
  FT_New_Face, or from bytes the application holds with FT_New_Memory_Face, instead of a Python file stream; the module-level FreeType library is created on first use,
  and faces are opened and closed under a lock (figures may be drawn on several threads at once);
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
  of Axis.get_minorticklocs, translated to C++ (the locator's axis is given as its tick space and minpos). Where
  tick_values raises (a view at or below zero with no positive data), the port notes Matplotlib's message
  (src/support/raised.hpp) and the drawing ends with it as an error.
- **Transforms** (src/transforms): transforms.py's Bbox, TransformedBbox, Affine2D, IdentityTransform,
  BboxTransformTo/From, BboxTransform, BlendedAffine2D, ScaledTranslation and the composite transforms, translated
  to C++ (matrices computed on demand rather than cached and invalidated; the same values). np.dot is reproduced
  (accumulating from +0.0) and numpy.linalg.inv by an LU solve that multiplies by pivot reciprocals as NumPy's
  OpenBLAS does; composites keep matplotlib's distinction between CompositeAffine2D and CompositeGenericTransform.
  str() of the boxes and of these transforms (_make_str_method, Bbox.__format__, Affine2D.__str__'s scale shorthand)
  as RendererSVG hashes it for clip path ids; a transform without it (the scales') returns nothing.
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
  literals (src/support/pyliteral), which leaves some spellings unread; dates (but the default epoch as it is
  written) and backend names are not validated but refused as not supported. The error messages are calculate-plot's.
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
  sticky edges yet) with MaxNLocator.view_limits, and every marker of markers.py with every fill style (the
  halves of a half-filled marker, drawn as Line2D.draw draws them, the other in markerfacecoloralt); the unit
  polygons and star the markers use, and the half width of the half-filled hexagon, are data
  (data/marker_paths.json) taken from matplotlib, since NumPy's vectorised sin and cos made them. Log scales: scale.py's LogScale (locators,
  formatters, limit_range_for_scale), Axis._set_axes_scale (with its immediate autoscale), minor ticks
  (Axis._update_ticks, get_minorticklocs), the data limits' minpos, autoscale_view through the scale's transform, and
  Axes.viewLim running a pending autoscale before it is read, translated to C++; mathtext texts are drawn and
  measured through the mathtext layout in both renderers. collections.py's Collection (colours with 'none' and
  'face', line widths, antialiasing, the draw with its single-path draw_markers optimisation and the hatch-colour
  probe call of draw_path_collection, the data limits of offset collections) and PathCollection (sizes), and
  axes/_axes.py's scatter with _parse_scatter_color_args and Axes.add_collection, translated to C++; the patches'
  own colour cycle is kept apart from the lines'. colors.py's Colormap.__call__, ListedColormap and
  LinearSegmentedColormap (_create_lookup_table, resampled, reversed; under/over/bad entries) and cm.py's registry
  with its reversed colormaps, and ScalarMappable.to_rgba, map scatter's values; data/colormaps.json holds every
  registered colormap's colours or segment data (the channels _cm.py computes with functions as their 256 values,
  so those are not resampled), with Collection.update_scalarmappable's choice of mapped faces and edges. PolyCollection (closed polygons) and
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
  Arguments: collections.py's Collection.set_color (the hatch colour with the faces and the edges) and
  get_hatchcolor's fallback on the kind's default edge colour; colors.py's _has_alpha_channel; axis.py's
  Tick.__init__ and Tick._apply_params for the grid lines (Axis.grid's keywords applied call after call, a colour
  with an alpha channel split into RGB and alpha); cbook.normalize_kwargs' refusal of a property given by two of
  its names; the order in which Line2D.__init__, Patch.__init__ (a color in place of edgecolor and facecolor)
  and Axes.vlines / hlines take their own parameters before the other keywords; Legend.__init__'s choice
  between ncols and ncol and numpy.array_split's reading of the count, translated to C++ in the executor.
  Property cycles: rcsetup.py's _parse_cycler_string and _eval_cycler_expr (a cycle written as text: calls of
  cycler() and concat(), +, *, slices, literals), cycler() with its three forms, validate_cycler, the
  validators of the properties of a cycle (_prop_validators, validate_color_for_prop_cycle,
  _single_string_color_list), translated to C++ (src/rc/cycler_text.cpp, src/rc/rc_validate.cpp). The text is
  read by a tokeniser and a parser of that grammar written here instead of by Python's ast module: what
  Python would read and they do not (string prefixes and escapes, sets, bytes, arithmetic between numbers,
  characters outside ASCII) is reported as not supported rather than refused. validate_cycler's second
  validation of the values is left out (they were validated when the cycle was made).
  The classic style: what rcParams["_internal.classic_mode"] changes in ticker.py (AutoLocator's bins and steps,
  LogLocator's number of ticks and choice of decades, LogFormatter's thresholds for minor labels), lines.py
  (Line2D.get_markeredgecolor), collections.py (Collection._bcast_lwls) and axes/_axes.py (the defaults of
  scatter and its margins, fill_between without normalised keywords or a colour from the cycle);
  Axes.set_axisbelow; legend_handler.py's HandlerNpoints.get_xdata, the handlers' use of numpoints,
  scatterpoints, scatteryoffsets and markerscale, and HandlerRegularPolyCollection.get_sizes, translated to C++.
  Style sheets: style/__init__.py's use (names, lists of names, the aliases, "default", the settings that are
  not about style left alone), translated to C++ (src/rc/styles); a figure file names only the bundled styles,
  never a file, a URL or a package. The bundled style sheets (mpl-data/stylelib/*.mplstyle) are data
  (data/styles.json; Solarize_Light2 in data/styles_solarized.json): each setting as the text the sheet gives,
  read as _rc_params_in_file reads a line, by tools/oracle/make_styles.py; the comments are left out.
  backend_agg.py's RendererAgg.draw_path: the chunks of rcParams["agg.path.chunksize"] (src/engine/path.cpp).
  The MOVETO codes matplotlib writes into the original path are written into the chunks only.
  Line2D's draw styles (cbook.py's pts_to_prestep, pts_to_poststep, pts_to_midstep), its subslice of long
  sorted lines (recache's condition, _x_filled with numpy.interp's formula, _path.is_sorted_and_has_non_nan,
  draw's searchsorted on the x bounds), markers drawn without the draw style and the path that recache then
  leaves as get_path(), and the same steps in fill_between's regions (_make_verts_for_region); markers.py's
  integer markers; the legend's numpoints, scatterpoints and markerscale keywords; cbook._combine_masks
  in scatter, hlines and vlines and np.ma.masked_invalid in fill_between, translated to C++.
  Hatches: hatch.py (get_path and the pattern classes, NumPy's linspace arithmetic; the unit star's
  vertices taken as data, data/marker_paths.json) translated to C++ (src/engine/hatch); Patch's and
  Collection's hatch, hatch colour and hatch line width, and HandlerPolyCollection's copy of them. Whether a
  colour's alpha is a Python int (to_rgba of an RGB tuple) is followed for the faces of patches, as the
  SVG ids of hatches hash its repr.
  pyplot's current figure and Axes (gcf, gca, sca through subplot), the functions of pyplot that act on
  them, Figure.add_subplot and SubplotSpec._from_subplot_args with GridSpec._check_gridspec_exists,
  Figure.add_axes and pyplot.axes, Axes.set, set_axis_off, loglog, semilogx and semilogy.
  Legends: the location as a code or a pair of coordinates (set_loc), and the shadow (patches.py's
  Shadow behind the frame, the frame's alpha of 1 when the keyword asks for a shadow).
  The schema of the calls (data/schema.json): the signatures of the calls and the setters, aliases and
  documented valid values of the artists (ArtistInspector), as data written by tools/oracle/make_schema.py.
  Bars: Axes.bar and barh (_parse_bar_color_args, the broadcasting, align, the error bars through
  errorbar), BarContainer and its legend entry (update_from_first_child), the sticky edges of
  autoscale_view.
  Bar widths through Axes._convert_dx (cbook._safe_first_finite).
  Histograms: Axes.hist for histtype 'bar' and 'barstacked' (cbook._reshape_2D for lists, the shared bin
  edges, stacking, density, cumulative, rwidth, align, orientation, bottom) and _style_helpers.style_generator
  (named line styles only), and for 'step' and 'stepfilled' the polygons' perimeters drawn by fill (in
  reverse order, sticky at 0, the step edges in the data sets' colours).
  Polygons: patches.py's Polygon (set_xy, set_closed), Axes.fill through _process_plot_var_args('Polygon')
  (_plot_args, _make_polygon, the patches' _PropCycle.getdefaults with ignored properties), pyplot.Polygon,
  Axes.add_patch with _update_patch_limits for paths (Path.iter_bezier over Path.cleaned, the segment ends
  as BezierSegment evaluates them), HandlerPatch for any patch and the 'best' legend placement around one.
  Annotations: text.py's _AnnotationBase and Annotation without arrows (src/engine/annotation.*: the
  coordinate systems by name or as a pair, _check_xy with Axes.contains_point, update_positions before drawing
  and measuring), Axes.annotate and pyplot.annotate; src/_path.h's point_in_path for one point (in
  src/agg/path_queries.h); Text.get_window_extent made virtual and non-const (an annotation places itself).
  Arrows: patches.py's ConnectionStyle (arc3, angle3, angle, arc, bar; clipping by patchA and shrinking),
  ArrowStyle (every curve, bracket and shape style; _Style's "name,key=value" strings and messages),
  FancyArrowPatch (drawn in display coordinates, its scale times points_to_pixels(1) of the last draw),
  Patch.contains and _draw_paths_with_artist_properties for several paths, Path.make_compound_path; bezier.py's
  routines in src/engine/bezier.* (BezierSegment at one parameter with OpenBLAS's order, de Casteljau,
  bisection, split_path_inout, parallel and wedged curves); Annotation's arrow (update_positions, the default
  patchA box, the arrow in its window extent). Figure files hold dicts as {"$dict": {...}}.
  Text boxes: BoxStyle (every style, with its checks and messages), FancyBboxPatch (also the legend's frame now),
  Text.set_bbox ("%0.2f" of the pad added to the style), set_backgroundcolor, update_bbox_position_size and
  _update_clip_properties (the text's clipping copied when drawn); an annotation's box is its arrow's patchA.
  Tick parameters: axis.py's Tick.__init__ keywords, Tick._apply_params, _apply_tickdir, Axis.set_tick_params
  (reset included), _translate_tick_params, _get_tick_label_size for the tick space, and Axes.tick_params.
  Twin axes: Axes.twinx / twiny and pyplot's, _make_twin_axes (add_subplot of the same cells, sharex/sharey with
  the other's limits and scale), _update_twinned_axes_patch_visibility; XAxis/YAxis set_ticks_position,
  tick_top/bottom/left/right, set_label_position, set_offset_position and the x offset text on top.
  Insets: Axes.inset_axes, add_child_axes and _TransformedBoundsLocator (the position at draw time), child Axes
  drawn among their parent's artists and counted in its tight box, and _update_title_position over the child Axes
  and twins (the top of an x axis whose ticks or label are on top).
  Inset indicators: inset.py's InsetIndicator (src/engine/inset.*: the rectangle, the connectors as
  ConnectionPatch makes them with arc3 and '-', which two show, one compound path in the rectangle's style),
  Axes.indicate_inset / indicate_inset_zoom; Figure._get_draw_artists placing every Axes and its child Axes
  before the figure draws.
  Symlog: scale.py's SymmetricalLogScale, SymmetricalLogTransform and its inverse (NumPy's elementwise formulas
  for one value), ticker.py's SymmetricalLogLocator (tick_values, view_limits) and LogFormatter.set_locs' symlog
  branch; set_xscale / set_yscale with base, linthresh, linscale and subs.
  Logit: scale.py's LogitScale (limit_range_for_scale, val_in_range), LogitTransform (mask, clip) and
  LogisticTransform, ticker.py's LogitLocator (major and minor, nonsingular, MaxNLocator's view_limits) and
  LogitFormatter (set_locs' choice of minor labels, _format_value, _is_decade), with nonpositive, one_half and
  use_overline.
  Log scales take base, subs and nonpositive (LogTransform's mask, np.log for base e; the minor formatter labels
  only bases when subs are given), also through loglog / semilogx / semilogy.
  Dates (src/axes/dates.*, with dateutil): dates.py's date2num and num2date (through datetime64[us], the default
  epoch, UTC only), AutoDateLocator.get_locator and its YearLocator, RRuleLocator and MicrosecondLocator (with
  ticker.py's MultipleLocator and _Edge_integer), DateLocator.nonsingular, AutoDateFormatter and DateFormatter
  with datetime.strftime's directives for dates in the C locale. The axes' units for dates: Axis.update_units
  with the DateConverter (_update_axisinfo the first time: the date tickers unless a FixedLocator is set) where
  _process_unit_info and plot's _plot_args call it (plot, step, scatter, fill_between(x), axhline / axvline after
  their bounds are read, axhspan / axvspan, hlines / vlines, set_xlim / set_ylim); a call is refused when it holds
  dates elsewhere or a list mixing dates with numbers; rcParams date.converter 'concise', another date.epoch and a
  timezone other than UTC are refused.
  Function scales: scale.py's FuncTransform, FuncScale and FuncScaleLog (FuncTransform + LogTransform, a LogScale
  for the locators, formatters and limit_range_for_scale), over the functions of figure files
  (src/support/pylambda.*); the 'log' name checks of Axis._set_lim and autoscale_view stay the log scale's only.
  Secondary axes: axes/_secondary_axes.py's SecondaryAxis (location and its _TransformedBoundsLocator,
  set_functions, _set_scale with the functions the other way round, _set_lims through the forward function,
  set_alignment, set_color, set_xticks / set_yticks keeping their ticks) and Axes.secondary_xaxis /
  secondary_yaxis; the other axis' NullLocator is an empty FixedLocator (the same ticks and limits). Functions
  are figure files' {"$lambda": ...} only (a Transform is refused), and calls on a secondary axis other than
  its labels, tick_params, ticks, colour and alignment are refused.
  Layout engines: layout_engine.py's ConstrainedLayoutEngine (compress too), TightLayoutEngine and
  PlaceHolderLayoutEngine, Figure.set_layout_engine and the layout, constrained_layout and tight_layout
  keywords of Figure, run by Figure.draw; print_figure's draw with drawing disabled before a save (the layout
  done there, then turned off while printing). _constrained_layout.py (do_constrained_layout,
  make_layoutgrids for the figure and its gridspecs, get_margin_from_padding, make_layout_margins,
  make_margin_suptitles for the suptitle, match_submerged_margins, check_no_collapsed_axes, reposition_axes,
  compress_fixed_aspect, reset_margins) and _layoutgrid.py's LayoutGrid, over kiwisolver's C++ solver with the
  expressions built as kiwisolver's Python module builds them. Not ported: subfigures, colorbars, figure
  legends, supxlabel / supylabel, nested gridspecs, Axes out of the layout.
  Bar labels: Axes.bar_label (BarContainer.datavalues, orientation and errorbar kept with the container,
  LineCollection.get_segments, Patch.get_window_extent, Bbox.intersection) and cbook._auto_format_str, with
  Python's % formatting and str.format of one number ported in src/support/pyformat.cpp (our own code: the
  rules of the Python language, checked against an oracle).
  Steps: Axes.step (plot with drawstyle 'steps-<where>'), Axes.stairs and patches.StepPatch (_update_path with
  cbook.contiguous_regions), HandlerStepPatch; Patch.__init__'s order of parameters shared by Polygon and
  StepPatch.
  Colormapping: where vmin equals vmax, Normalize's result.fill(0) leaves masked values masked (NaN here).
  Stems: Axes.stem (vlines/hlines, plot and a StemContainer) and HandlerStem; collections take the line
  styles 'None', 'none', '' and ' ' as undashed (lines._get_dash_pattern).
  Box plots: cbook.boxplot_stats (without bootstrap), Axes.boxplot and Axes.bxp (merge_kw_rc over
  rcParams["boxplot.*"], PathPatch boxes by add_artist, manage_ticks), ticker.py's FixedLocator and
  FixedFormatter as the axis' major locator and formatter.
  Axes.fill_betweenx (FillBetweenPolyCollection with t_direction 'y', _fix_pts_xy_order).
  Violins: mlab.GaussianKDE (one-dimensional), cbook.violin_stats and delete_masked_points (src/engine/violin.*),
  Axes.violinplot and Axes.violin (the classic style is refused); Collection.set_capstyle for LineCollections.
  Aspect: Axes.set_aspect with adjustable 'box' and apply_aspect (get_data_ratio, Bbox.shrunk_to_aspect and
  anchored) at the start of draw and get_tightbbox; the original and the active position kept apart.
  Ticks: Axis.set_ticks (FixedLocator, set_view_interval without ignore on the shared axes) and set_ticklabels
  (NullFormatter, or a FuncFormatter of a dict of labels); set_frame_on.
  Pies: Path.arc, patches.Wedge, Shadow's zorder, Axes.pie and Axes.pie_label; the data limits of curved
  paths (bezier.BezierSegment: polynomial coefficients, axis-aligned extrema by _real_roots_in_01 and
  _quadratic_roots_in_01, evaluation as NumPy's matrix product makes it).
  Colour sequences: colors.py's ColorSequenceRegistry (the built-in sequences only) with _cm.py's data
  (data/color_sequences.json; ColorBrewer's schemes in data/color_sequences_colorbrewer.json), written as JSON
  numbers by tools/oracle/make_color_sequences.py; rcsetup.py's _single_string_color_list takes them in cycles.
- **Image resampling** (src/agg/image_resample.h, src/render/resample.cpp): src/_image_resample.h unchanged but for
  its location; src/_image_wrapper.cpp's image_resample (pixel types by dtype, the parameters taken as C floats,
  the lookup mesh of a non-affine transform given by the caller) translated to a C++ function without Python. The
  build defines MPL_FIX_AGG_IMAGE_FILTER_LUT_BUGS and MPL_FIX_AGG_INTERPOLATION_ENDPOINT_BUG for Agg, as matplotlib's
  meson build does.
- **Colorbars** (src/engine/colorbar.cpp): colorbar.py's Colorbar (_process_values, _mesh, _proportional_y,
  _uniform_y, _do_extends, _update_dividers, _add_solids, _add_solids_patches, _locate, add_lines for a contour set
  of lines, the scale and ticks of the long axis, a contour set as the mappable),
  _ColorbarAxesLocator, _ColorbarSpine, make_axes and make_axes_gridspec, and figure.py's Figure.colorbar,
  translated to C++; gridspec.py's GridSpecFromSubplotSpec, Axes.set_box_aspect and set_subplotspec, and
  backend_mixed.py's MixedModeRenderer.start_rasterizing / stop_rasterizing (src/engine/figure.cpp) for rasterized
  artists in SVG.
- **Contour sets** (src/engine/contour_set.cpp, src/engine/engine.cpp contour): contour.py's ContourSet and
  QuadContourSet (_process_args, _contour_args, _check_xyz, _initialize_x_y, _process_contour_level_args, _autolev,
  _process_levels, _process_colors, _process_linewidths, _process_linestyles, _get_lowers_and_uppers, the paths
  from the contour generator, draw with hatches), cbook.py's _resize_sequence and axes/_axes.py's Axes.contour and
  Axes.contourf, translated to C++; ContourLabeler (src/engine/contour_labels.cpp: clabel with automatic
  placement, get_text, _get_nth_label_width, print_label, too_close, locate_label, _split_path_and_get_label_rotation,
  add_label, labels) and path.py's _iter_connected_components and make_compound_path, translated to C++ with NumPy's
  operations in their order; np.argsort's order of equal distances (x86-simd-sort's) is not reproduced: a choice
  between equally straight blocks is refused; colors.py's ListedColormap under
  and over colours and its monochrome property (src/colors/colormap.cpp).
- **Hexagonal binning** (src/engine/engine.cpp hexbin): axes/_axes.py's Axes.hexbin (the grid, the two lattices,
  bincount or reduce_C_function per hexagon, mincnt, bins, the log scales, the marginals with np.geomspace /
  np.linspace edges) and cbook.delete_masked_points for its data, translated to C++ with NumPy's operations in their
  order; transforms.py's AffineDeltaTransform (src/transforms/transforms.cpp). reduce_C_function is one of np.mean,
  np.sum, np.max, np.min, np.median and len, named in figure files with {"$func": name}.
- **Quivers and barbs** (src/engine/quiver.cpp, src/engine/engine.cpp quiver, quiverkey and barbs): quiver.py's
  Quiver (_parse_args, set_UVC, _init, _dots_per_unit, _set_transform, _angles_lengths, _make_verts, _h_arrows, draw),
  QuiverKey (_init, _text_shift, draw) and Barbs (set_UVC, _find_tails, _make_barbs, with patches.py's CirclePolygon
  vertices and _CollectionWithSizes' sizes), and axes/_axes.py's Axes.quiver, Axes.quiverkey and Axes.barbs, translated to C++ with NumPy's operations
  in their order (NumPy's complex multiplication and absolute value from src/support/npy_complex.cpp).
- **Streamlines** (src/engine/streamplot.cpp, src/engine/engine.cpp streamplot): streamplot.py's streamplot, Grid,
  StreamMask, DomainMap, _get_integrator, _integrate_rk12, _euler_step, interpgrid and _gen_starting_points,
  translated to C++ with the exceptions as return values; the arrows as FancyArrowPatch objects in data coordinates
  (patches.py's FancyArrowPatch.get_path back through its transform, src/engine/arrows.cpp).
- **Polar projection** (src/engine/polar.cpp): projections/polar.py's PolarTransform (transform_non_affine and
  transform_path_non_affine, with path.py's Path.iter_segments through the cleanup_path port and cbook.py's
  simple_linear_interpolation), InvertedPolarTransform, PolarAffine and _WedgeBbox (with patches.py's Wedge of a
  width), translated to C++; the axis a transform asks for its radial origin is given as the Axes' lockable origin
  box (transforms.py's LockableBbox, src/transforms/transforms.hpp). CompositeGenericTransform's and
  TransformWrapper's transform_path_non_affine (src/engine/path.cpp) are followed for transforms with a polar part.
  PolarAxes (_set_lim_and_transforms, clear, draw, the axis and text transforms, get_data_ratio), _ThetaShift,
  ThetaLocator, ThetaFormatter, RadialLocator, ThetaTick's and RadialTick's update_position and the theta and radial
  axes' clear and _set_scale are translated into the library's one Axes class, which takes a polar state
  (src/engine/polar.cpp, src/engine/axes.cpp); spines.py's arc spines (set_patch_arc, _adjust_location of arcs) join
  its Spine; the polar clip path is the live Wedge patch (TransformedPatchPath). PolarAxes' setters (set_theta_offset,
  set_theta_zero_location, set_theta_direction, set_thetamin / set_thetamax / set_thetalim, set_rorigin, set_rmin /
  set_rmax / set_rlim, set_rlabel_position, set_rscale, set_rticks, set_rgrids, set_thetagrids) are calls of the
  figure files (src/engine/engine.cpp polarCall). The calls on polar Axes follow axes/_axes.py's polar paths: the
  _interpolation_steps bar (100) and axhspan / axvspan (180) set on the shared unit rectangle (a figure-wide value
  here), axhline / axvline and stem baselines (180), the theta error bars (2), errorbar's caps one line per point
  with the MarkerStyle(transform=_ScaledRotation) turn, the non-rectilinear data limits of _update_line_limits,
  _update_patch_limits, vlines and hlines; pyplot.polar; PolarAxes.start_pan / drag_pan / can_zoom and format_coord
  in src/engine/navigation.cpp (the toolbar's result as set_rlabel_position / set_rmax calls).
- **Delaunay triangulation** (src/tri/delaunay.cpp): src/_qhull_wrapper.cpp's delaunay and delaunay_impl with its
  helpers, without pybind11: the arrays are vectors, the exceptions return values; Qhull's messages go to /dev/null as
  when Python is not verbose. tri/_triangulation.py's Triangulation and src/tri/_tri.cpp's Triangulation
  (calculate_edges, calculate_neighbors, calculate_boundaries, calculate_plane_coefficients, correct_triangles) are one
  class (src/tri/triangulation.cpp) without pybind11; tri/_triplot.py's triplot and tri/_tripcolor.py's tripcolor
  are in src/engine/engine.cpp, collections.py's TriMesh in src/engine/collections.cpp. src/tri/_tri.cpp's
  TriContourGenerator is src/tri/tri_contour.cpp (vectors for the NumPy arrays); tri/_tricontour.py's TriContourSet
  (_contour_args) joins src/engine/contour_set.cpp's ContourSet as its triangulation input.
- **Stacked areas** (src/engine/engine.cpp stackplot): stackplot.py's stackplot with _style_helpers.py's
  style_generator, the bands through Axes.fill_between. axes/_axes.py's broken_barh, with collections.py's
  Collection.get_datalim for PolyCollection (the paths' extents when the transform contains transData).
  axes/_axes.py's axline with lines.py's AxLine (get_transform made at each drawing; src/engine/lines.cpp), and
  eventplot with collections.py's EventCollection (set_positions) as a LineCollection, and arrow with
  patches.py's FancyArrow (_make_verts; np.dot as BLAS's dgemm sums) as a Polygon.
- **ecdf** (src/engine/engine.cpp ecdf): axes/_axes.py's ecdf, a stepped line through Axes.plot. xcorr and acorr
  through vlines / axhline or plot (np.correlate in src/support/npy_correlate.cpp).
- **Spectra** (src/engine/spectral.cpp): mlab.py's window_hanning, window_none, _spectral_helper,
  _single_spectrum_helper, psd, csd, specgram and cohere, translated to C++ with NumPy's operations in their order
  (np.hanning, sliding windows, np.fft.fft through src/support/npy_fft.cpp, complex products and quotients,
  pairwise sums of real and complex windows, the mean over Fortran-ordered segments, np.fft.fftfreq, np.roll,
  np.unwrap, np.arange of floats); detrending other than none is not ported yet. axes/_axes.py's psd, csd, cohere,
  magnitude_spectrum, angle_spectrum, phase_spectrum (lines through Axes.plot, with psd's and csd's integer y ticks)
  and specgram (an image through Axes.imshow, then _base.py's axis('auto')) in src/engine/engine.cpp spectrum.
- **Tables** (src/engine/table.cpp, src/engine/engine.cpp table): table.py's Cell, Table (alignment, auto font size,
  auto column width, placement by loc or bbox) and table(), with axes/_base.py's add_table, translated to C++.
- **Meshes** (src/engine/collections.cpp QuadMesh and PolyQuadMesh, src/engine/engine.cpp pcolormesh, pcolor,
  pcolorfast; src/engine/image.cpp PcolorImage): collections.py's _MeshData, PolyQuadMesh and QuadMesh (_convert_mesh_to_triangles for Gouraud shading, draw), backend_bases.py's RendererBase.draw_quad_mesh
  (the SVG renderer's path collection of the cells), axes/_axes.py's pcolormesh, pcolor, pcolorfast,
  _pcolorargs (with _interp_grid) and _update_pcolor_lims, image.py's PcolorImage, translated to C++.
- **Images** (src/engine/image.cpp): image.py's _ImageBase, AxesImage and FigureImage (_normalize_image_array, set_alpha of
  arrays, _make_image with both interpolation stages, the unsampled path, get_extent / set_extent, draw), _resample,
  composite_images and _draw_list_compositing_images (for Axes and the figure), colorizer.py's _pass_image_data,
  cbook.safe_masked_invalid, axes/_axes.py's imshow, matshow and spy, and figure.py's figimage, translated to C++ with NumPy's dtype rules step by step; the PNG encoder deflates as
  Pillow's does (level 6, memory level 9, Z_FILTERED), so embedded images are byte-identical.
- **Norms** (src/colors/norms.cpp): colors.py's Normalize (process_value's dtypes, autoscale_None, clip, inverse),
  TwoSlopeNorm, CenteredNorm (its linked vmin / vmax setters), PowerNorm, BoundaryNorm (np.digitize as a sorted
  search), NoNorm and the norms make_norm_from_scale builds (LogNorm, SymLogNorm, AsinhNorm, and the norms
  colorizer.py's _auto_norm_from_scale makes from a scale name), with scale.py's transforms they use, translated to
  C++ over one-dimensional masked arrays; NumPy's type rules (float32 kept by in-place and unary operations, float64
  for masked arithmetic with Python numbers) are followed step by step; a symlog norm of float32 data takes NumPy's
  own float32 logarithm (src/support/npy_logf.cpp).
- **Screens** (src/engine/figure.cpp drawScreenTrace): what backend_bases.py's _set_device_pixel_ratio and
  backend_qt.py's FigureCanvasQT.resizeEvent do to the figure (the dpi times the ratio; set_size_inches of the
  widget's size times the ratio over that dpi, forward=False), translated to C++ without Qt; the application
  draws the image itself.
- **Navigation** (src/engine/navigation.cpp): NavigationToolbar2's pan and zoom (press_pan, drag_pan,
  release_zoom, _start_event_axes_interaction), the Axes' start_pan, _get_pan_points, _prepare_view_from_bbox,
  _set_view_from_bbox and set_xbound / set_ybound, and FigureCanvasQT.mouseEventCoords with LocationEvent's
  int() of the position, translated to C++ for one gesture from press to release; the view the gesture leaves is
  returned as set_xlim / set_ylim calls of the figure file instead of being set on a live figure. The pointer's
  text: _mouse_event_to_message, FigureCanvasBase.inaxes, format_coord and the formatters' format_data_short
  (ScalarFormatter with cbook._g_sig_digits, LogFormatter, LogitFormatter; AutoDateFormatter, NullFormatter,
  FixedFormatter and FuncFormatter through __call__ without a position) in src/engine/axes.cpp.
- File names drop the leading underscore (e.g. src/_backend_agg.h is src/agg/backend_agg.h); includes follow.
- RendererAgg's rasterizer takes Matplotlib's 32 768 blocks of cells on 64-bit targets and 4 096 on 32-bit ones
  (WebAssembly), which have no memory for more: a path that needs more is refused there with the same
  "Exceeded cell block limit".
- The drawing algorithms themselves (src/_backend_agg.h, src/_backend_agg.cpp, src/path_converters.h,
  src/agg_workaround.h, src/array.h, src/mplutils.h) are otherwise unchanged.
