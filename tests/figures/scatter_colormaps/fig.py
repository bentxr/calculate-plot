# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 3, figsize=(7.0, 4.0))
axs[0, 0].scatter([0, 1, 2, 3, 4, 5, 6, 7], [1, 3, 2, 5, 4, 6, 0, 2], c=[0.5, 2.25, 1, 7, 3.5, 0, -1, 9], s=60, cmap='jet')
axs[0, 1].scatter([0, 1, 2, 3, 4, 5], [1, 3, 2, 5, 4, 6], c=[0, 1, 2, 3, 4, 5], cmap='tab20_r', s=120, marker='s')
axs[0, 2].scatter([0, 1, 2, 3, 4, 5], [1, 3, 2, 5, 4, 6], c=[10, 20, 30, 40, 50, 60], cmap='gnuplot2', vmin=15, vmax=55, edgecolors='k')
axs[1, 0].scatter([0, 1, 2, 3, 4, 5], [1, 3, 2, 5, 4, 6], c=[0.1, 0.2, 0.3, 0.4, 0.5, 0.6], cmap='twilight_shifted', alpha=0.5, s=200)
axs[1, 1].scatter([0, 1, 2, 3, 4, 5], [1, 3, 2, 5, 4, 6], c=[3, 1, 4, 1, 5, 9], cmap='cubehelix_r', s=90)
axs[1, 2].scatter([0, 1, 2, 3, 4, 5], [1, 3, 2, 5, 4, 6], c=[3, 1, 4, 1, 5, 9], cmap='Pastel1', facecolors='none', s=150, linewidths=3)

if __name__ == "__main__":
    plt.show()
