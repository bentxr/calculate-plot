# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(6.4, 3.2))
axs[0].fill_between([0, 1, 2, 3], [0, 1, 0.5, 2], [2, 3, 3, 4], hatch='//')
axs[0].fill_between([0, 1, 2, 3], [-1, -2, -1, -3], hatch='x', alpha=0.5, color='C2')
axs[0].fill_between([0, 1, 2, 3], [4, 5, 4, 5], 6, hatch='o.', edgecolor='C3', facecolor='none')
axs[1].fill_between([0, 1, float('nan'), 3, 4], [1, 2, 1, 2, 1], hatch='*', linewidth=2)
axs[1].fill_between([0, 4], [3, 3], 4, hatch='')
axs[1].fill_between([0, 4], [5, 5], 6, hatch='-|', color='0.6')

if __name__ == "__main__":
    plt.show()
