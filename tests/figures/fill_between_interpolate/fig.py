# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2)
axs[0, 0].plot([0, 1, 2, 3, 4, 5], [0, 2, -1, 3, 1, -2])
axs[0, 0].fill_between([0, 1, 2, 3, 4, 5], [0, 2, -1, 3, 1, -2], 0.5, where=[True, True, False, True, True, False], interpolate=True, alpha=0.5)
axs[0, 0].fill_between([0, 1, 2, 3, 4, 5], [0, 2, -1, 3, 1, -2], 0.5, where=[False, False, True, False, False, True], interpolate=True, color='C1')
axs[0, 1].fill_between([0, 1, 2, 3, 4, 5, 6], [1, 3, float('nan'), 2, 0.1, 4, 2], [2, 2, 2, 2, 2, 2, 2], where=[True, True, True, False, False, True, True], interpolate=True)
axs[0, 1].fill_between([0, 1, 2, 3, 4, 5, 6], [1, 3, 2.5, 2, 0.1, 4, 2], [2, 2, 2, 2, 2, 2, float('nan')], where=[False, False, False, True, True, False, True], interpolate=True, step='mid', color='C2')
axs[1, 0].fill_betweenx([0, 1, 2, 3], [0, 2, -1, 1], [1, 1, 1, 1], where=[True, True, False, True], interpolate=True, step='pre')
axs[1, 1].fill_between([3, 2, 1, 0], [1, 1, 0, 0], [0, 1, 1, 0], where=[True, False, True, True], interpolate=True)
axs[1, 1].fill_between([0, 1, 2], [5, 6, 7], interpolate=True, where=[False, True, False])

if __name__ == "__main__":
    plt.show()
