# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 1)
axs[0].bar([-0.5, -1, 25.0], [0.2, 0.3, 0], bottom=[-39.2, float('nan'), -0.5])
axs[1].barh([1, 2, 3, 4], [2.5, float('nan'), 1.5, 3], left=[float('nan'), 0.5, -1.25, 2])

if __name__ == "__main__":
    plt.show()
