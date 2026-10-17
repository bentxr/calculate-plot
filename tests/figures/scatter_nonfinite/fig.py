# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(8.0, 3.0))
axs[0].scatter([1, float('nan'), 3, 4, 5], [2, 1, float('inf'), 3, float('-inf')], label='x or y bad')
axs[0].scatter([-35.5, 0.2, 7], [28.152176, -42, 3], s=[25.0, float('nan'), 64], marker='d', label='a size bad')
axs[0].legend(loc='upper left')
axs[1].scatter([0, 1, 2, 3, 4, 5], [5, 4, 3, 2, 1, 0], c=[0.5, float('nan'), 2, 3, 40, -1], s=80)
axs[1].scatter([0, 1, float('nan'), 3], [1, 1, 1, 1], c=[1, 2, 3, 4], cmap='viridis')
axs[2].set_xscale('log')
axs[2].scatter([0.5, 1, float('nan'), 10, 1e-07, 42], [1, 2, 3, 4, 5, 6], s=30)
axs[2].scatter([3], [float('nan')], label='all bad')
axs[2].legend()

if __name__ == "__main__":
    plt.show()
