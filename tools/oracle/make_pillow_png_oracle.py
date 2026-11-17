#!/usr/bin/env python3
"""Write tests/oracle/pillow_png.json: what Pillow 12.3.0 (the version matplotlib 3.11.2 runs with in the
oracle environment) does with the images matplotlib's PDF backend hands it: Image.getcolors(maxcolors=256)
of RGB images (its list, in its order, or None), and the bytes of Image.save(format="png") for L, RGB, RGBA
and P images (P with palettes of 1 to 256 colours, so that 1, 2, 4 and 8 bit rows are written).

  tools/oracle/.venv/bin/python -I tools/oracle/make_pillow_png_oracle.py
"""
import base64
import io
import json
import pathlib

import numpy as np
import PIL
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert PIL.__version__ == "12.3.0"


def png(img):
    buffer = io.BytesIO()
    img.save(buffer, format="png")
    return base64.b64encode(buffer.getvalue()).decode("ascii")


def array(a):
    return {"shape": list(a.shape), "base64": base64.b64encode(np.ascontiguousarray(a).tobytes()).decode("ascii")}


rng = np.random.default_rng(20261117)
cases = []

# RGB images: few colours (indexed by matplotlib), many (> 256), smooth gradients, solid, one row / column
rgb_images = [
    rng.integers(0, 256, (7, 5, 3), dtype=np.uint8),
    rng.integers(0, 256, (40, 33, 3), dtype=np.uint8),
    np.zeros((3, 4, 3), np.uint8),
    np.full((2, 9, 3), 200, np.uint8),
]
palette = rng.integers(0, 256, (300, 3), dtype=np.uint8)
for n in (1, 2, 3, 4, 5, 16, 17, 100, 255, 256, 257):
    for shape in ((6, 7), (1, 13), (19, 1), (16, 16)):
        rgb_images.append(palette[:n][rng.integers(0, n, shape)])
ramp = np.linspace(0, 255, 64).astype(np.uint8)
rgb_images.append(np.stack(np.broadcast_arrays(ramp[None, :], ramp[:, None], np.uint8(90)), axis=-1))
for im in rgb_images:
    img = Image.fromarray(im)
    colors = img.getcolors(maxcolors=256)
    cases.append({"mode": "RGB", "image": array(im),
                  "getcolors": None if colors is None else [[c, list(rgb)] for c, rgb in colors],
                  "png": png(img)})

# L images (matplotlib's soft masks and greyscale data)
for shape in ((5, 6), (1, 9), (17, 3), (30, 31)):
    im = rng.integers(0, 256, shape, dtype=np.uint8)
    cases.append({"mode": "L", "image": array(im), "png": png(Image.fromarray(im))})
im = np.tile(np.arange(0, 256, 4, dtype=np.uint8), (4, 1))
cases.append({"mode": "L", "image": array(im), "png": png(Image.fromarray(im))})

# RGBA images (the PNGs embedded in SVG files)
for shape in ((4, 5), (23, 17), (1, 1), (9, 40)):
    im = rng.integers(0, 256, shape + (4,), dtype=np.uint8)
    im[..., 3] = np.where(rng.random(shape) < 0.3, 255, im[..., 3])
    cases.append({"mode": "RGBA", "image": array(im), "png": png(Image.fromarray(im))})

# P images as matplotlib makes them: Image.fromarray(indices).convert("P"), then putpalette of n colours
for n in (1, 2, 3, 4, 5, 15, 16, 17, 64, 255, 256):
    for shape in ((6, 7), (1, 13), (19, 1), (11, 23)):
        indices = rng.integers(0, n, shape).astype(np.uint8)
        pal = rng.integers(0, 256, n * 3, dtype=np.uint8)
        img = Image.fromarray(indices).convert("P")
        img.putpalette(pal)
        cases.append({"mode": "P", "image": array(indices), "palette": base64.b64encode(pal.tobytes()).decode("ascii"),
                      "png": png(img)})

out = ROOT / "tests" / "oracle" / "pillow_png.json"
out.write_text(json.dumps({"pillow": PIL.__version__, "cases": cases}, separators=(",", ":")) + "\n")
print(f"{out}: {len(cases)} cases")
