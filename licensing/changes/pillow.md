# Changes to Pillow code (MIT-CMU: copies keep the copyright notice and the permission notice)

src/png/pillow_png.hpp and src/png/pillow_png.cpp contain, translated to C++ from Pillow 12.3.0, what Pillow does
with the images matplotlib saves through it, so that calculate-plot writes the same bytes:
- getcolors32 from src/libImaging/GetBBox.c (Image.getcolors of an RGB image): its hash table and the order in
  which it lists the colours, which decides the palette of an indexed image in a PDF file;
- the PNG writing of Image.save(format="png") for L, RGB, RGBA and palette images: PngImagePlugin._save's choice
  of 1, 2, 4 or 8 bits for a palette and its chunks, ImageFile._save's blocks (one IDAT chunk each), the packers
  pack1, packP2 and packP4 from src/libImaging/Pack.c, and ZipEncode.c's row filters (none, up, sub, Paeth; none
  for 8-bit palette data, as encode.c's ZIP_PNG_PALETTE mode) and deflate settings.
Pillow's image objects, modes other than those, and its other encoders are not included; the deflate itself is
the vendored zlib's.
