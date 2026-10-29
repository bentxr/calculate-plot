# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(figsize=(7.0, 5.0))
axd = fig.subplot_mosaic('\n    xy\n    xz\n    ', empty_sentinel='z', subplot_kw={'projection': 'rectilinear'})
axd['x'].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axd['y'].hist([1.1, -0.4, 2.8, 0.9, 3.7])

if __name__ == "__main__":
    plt.show()
