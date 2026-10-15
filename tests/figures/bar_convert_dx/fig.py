# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2)
axs[0, 0].bar([10, 0.2], [-29.79, -0.5])
axs[0, 1].barh([0.3, 1.7], [2.5, -1.1], left=[0.1, 3.3], xerr=0.35, yerr=[0.1, 0.2])
axs[1, 0].bar([float('nan'), 1.1, 2.3], [1, 2, 3], bottom=[0.7, 1.3, 2.9], width=[0.3, 0.45, 0.6], yerr=[[0.1, 0.2, 0.3], [0.3, 0.2, 0.1]], xerr=[0.05, 0.15, 0.25])
axs[1, 1].bar([100000.0, 100001.3], [1, 2], width=0.7, align='edge', bottom=0.1)

if __name__ == "__main__":
    plt.show()
