# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2)
axs[0].fill_betweenx([0, 1, 2, 3], [1, 2, 1.5, 3], label='x1 only')
axs[0].fill_betweenx([0, 1, 2, 3], [4, 5, 4.5, 6], 3.5, step='pre', hatch='//', edgecolor='k', alpha=0.5, label='step')
axs[0].legend()
axs[1].fill_betweenx([0, 1, 2, 3, 4], x1=[1, float('nan'), 2, 3, 2], x2=[0, 0, 1, 1, 0], where=[True, True, True, False, True], facecolor='C4', edgecolor=None)
axs[1].fill_between([5, 6, 7], [1, 2, 1], facecolor=None, color=None, edgecolor=None)
axs[1].fill_betweenx([0, 1, 2], [6, 7, 8], 5.5, step='mid', color='g')

if __name__ == "__main__":
    plt.show()
