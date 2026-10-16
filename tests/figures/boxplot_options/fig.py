# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(7.2, 3))
axs[0].plot([0, 4], [0, 10])
axs[0].boxplot([[1, 3, 4, 6, 9], [2, 4, 4, 5, 7, 12]], positions=[1, 3], showfliers=False, showbox=False, zorder=5)
axs[0].boxplot([[3, 4, 5, 6]], positions=[2], tick_labels=['mid'], sym='')
axs[1].boxplot([[1, 2, float('nan'), 4], [1, 2, 3, 4, 5]], usermedians=[None, 3.5], conf_intervals=[None, [2, 4]], notch=True)
axs[2].boxplot([[1, 2, 3, 30], []], manage_ticks=False, patch_artist=True)
axs[2].set_yscale('log')

if __name__ == "__main__":
    plt.show()
