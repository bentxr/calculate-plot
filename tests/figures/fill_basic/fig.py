# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.2, 5.4))
axs[0, 0].fill([0, 1, 2], [0, 1, 0])
axs[0, 0].fill([0, 1, 2], [1, 2, 1], 'r', [3, 4, 5], [0, 1, 0], label='two')
axs[0, 0].fill([3, 4, 5], [1, 2, 1], label='third', alpha=0.5)
axs[0, 0].legend()
axs[0, 1].fill([0, 1, 2, 1], [0, 0, 1, 2], facecolor='C3', edgecolor='k', linewidth=2, hatch='//', alpha=0.5)
axs[0, 1].fill([2, 3, 4, 2], [0, 1, 0, 0], closed=False, fill=False, ec='b', ls='--')
axs[0, 1].fill([0, 2, 4], [2, 3, 2], c='g', zorder=3, lw=0.5)
axs[1, 0].fill([0, 1, 2, 3], [0, float('nan'), 1, 0])
axs[1, 0].fill([-0.0, 1, -0.0], [-0.0, 1, 1], facecolor='none', edgecolor='C1')
axs[1, 0].fill([5], [2], edgecolor='k')
axs[1, 0].fill([], [])
axs[1, 1].fill([1, 3, 2])
axs[1, 1].fill([0, 1, 2], [3, 4, 3], 'g--')
axs[1, 1].fill([0, 1, 2, 0], [0, -1, 0, 0], closed=None, hatch='x', fill=False, edgecolor=None)
axs[1, 1].fill([3, 4, 5], [0, -1, 0], closed=0, facecolor=None, linewidth=None, zorder=None)

if __name__ == "__main__":
    plt.show()
