# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt
import numpy as np

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(6.0, 2.2))
axs[0].imshow(np.array([[0, 7, 14, 21, 28, 35], [40, 47, 54, 61, 68, 75], [80, 87, 94, 101, 108, 115], [120, 127, 134, 141, 148, 155], [160, 167, 174, 181, 188, 195], [200, 207, 214, 221, 228, 235]], dtype=np.uint8), cmap='gray')
axs[1].imshow(np.array([[-600, -589, -578, -567, -556, -545, -534], [-300, -289, -278, -267, -256, -245, -234], [0, 11, 22, 33, 44, 55, 66], [300, 311, 322, 333, 344, 355, 366], [600, 611, 622, 633, 644, 655, 666]], dtype=np.int16))
axs[2].imshow(np.array([[0.0, 0.37, 0.74, 1.11], [0.1, 0.47, 0.84, 1.21], [0.2, 0.57, 0.94, 1.31], [0.3, 0.67, 1.04, 1.41]], dtype=np.float32), interpolation='gaussian')

if __name__ == "__main__":
    plt.show()
