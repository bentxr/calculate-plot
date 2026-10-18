# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(7.0, 2.5))
axs[0].pcolorfast([[0, 1, 2, 3, 4, 5, 6, 0], [1, 2, 3, 4, 5, 6, 0, 1], [2, 3, 4, 5, 6, 0, 1, 2], [3, 4, 5, 6, 0, 1, 2, 3], [4, 5, 6, 0, 1, 2, 3, 4], [5, 6, 0, 1, 2, 3, 4, 5]])
axs[1].pcolorfast([0, 1, 1.5, 4, 4.2], [0, 0.5, 2, 3], [[0, -1, -2, -3], [1, 0, -1, -2], [2, 1, 0, -1]], cmap='RdYlBu')
axs[2].pcolorfast([[0.0, 1.0, 2.0, 3.0, 4.0], [0.2, 1.2, 2.2, 3.2, 4.2], [0.4, 1.4, 2.4, 3.4, 4.4], [0.6000000000000001, 1.6, 2.6, 3.6, 4.6]], [[0.0, 0.1, 0.2, 0.30000000000000004, 0.4], [1.0, 1.1, 1.2, 1.3, 1.4], [2.0, 2.1, 2.2, 2.3, 2.4], [3.0, 3.1, 3.2, 3.3, 3.4]], [[0, 0, 0, 0], [0, 1, 2, 3], [0, 2, 4, 6]], vmin=0, vmax=4)

if __name__ == "__main__":
    plt.show()
