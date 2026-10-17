# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(6.4, 3.2))
axs[0].fill_between([-1.7, -0.5, 1e-07, 0.001, 100], [-46.4, float('nan'), 41.2, 0.2, -43.964777], [2, 0.2, 0, 100, 7.25], label='one point, then three')
axs[0].fill_between([0, 1, 2, 3, 4, 5, 6], [1, 2, float('inf'), 2, 1, 3, 2], alpha=0.5)
axs[0].legend()
axs[1].fill_between([0, 1, float('nan'), 3, 4, 5, 6, 7], [1, 2, 3, 2, 1, 3, 2, 1], [0, 0, 0, float('-inf'), 0, 0, 0, 0], where=[True, True, True, True, True, False, True, True])
axs[1].fill_between([0, 1, 2], [float('nan'), float('nan'), float('nan')], 5, color='C4')

if __name__ == "__main__":
    plt.show()
