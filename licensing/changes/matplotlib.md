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
- **Transforms** (src/transforms): transforms.py's Bbox, TransformedBbox, Affine2D, IdentityTransform,
  BboxTransformTo/From, BboxTransform, BlendedAffine2D, ScaledTranslation and the composite transforms, translated
  to C++ (matrices computed on demand rather than cached and invalidated; the same values). np.dot is reproduced
  (accumulating from +0.0) and numpy.linalg.inv by an LU solve that multiplies by pivot reciprocals as NumPy's
  OpenBLAS does; composites keep matplotlib's distinction between CompositeAffine2D and CompositeGenericTransform.
- **Font manager** (src/text/font_manager, data/fonts.json): font_manager.py's FontProperties defaults, font
  size names, the scoring functions and _find_fonts_by_props/_findfont_cached, translated to C++, over a fixed list of
  the bundled fonts (dumped from matplotlib's FontManager, sorted by file name) instead of a scan of the system's
  fonts with a cache file; font files are named by file name.
- **Figures** (src/engine): path.py's Path (vertices, codes and the simplification settings),
  backend_bases.py's GraphicsContextBase and the RendererBase interface, artist.py's Artist (clip settings),
  patches.py's Patch and Rectangle, lines.py's dash patterns and figure.py's Figure (its boxes, transforms and
  patch) with backend_agg.py's and backend_svg.py's canvas drawing, translated to C++. Artists draw through a
  renderer interface; calculate-plot's renderers record the calls in its trace format instead of drawing
  directly. Colours are given as strings for now.
- File names drop the leading underscore (e.g. src/_backend_agg.h is src/agg/backend_agg.h); includes follow.
- The drawing algorithms themselves (src/_backend_agg.h, src/_backend_agg.cpp, src/path_converters.h,
  src/agg_workaround.h, src/array.h, src/mplutils.h) are otherwise unchanged.
