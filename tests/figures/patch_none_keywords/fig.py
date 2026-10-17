# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(7.2, 3.2))
axs[0].axvspan(0, 1, ec=None, facecolor='C1')
axs[0].axhspan(0.2, 0.4, fc=None, lw=None, ls=None, zorder=None, hatch=None, label=None)
axs[0].axvspan(2, 3, color='C2', fill=None, linewidth=3)
axs[0].axvspan(4, 5, color='C3', fill=0, edgecolor='k', alpha=None)
axs[1].bar([1, 2, 3], [3, 1, 2], ls=None, zorder=None, fill=1, label=None)
axs[1].hist([1, 2, 2, 3, 3, 3], bins=3, zorder=None, fill=None, edgecolor='k')

if __name__ == "__main__":
    plt.show()
