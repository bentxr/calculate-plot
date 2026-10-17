# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(8.0, 3.0))
axs[0].fill_between([0, 1, 2, 3.5, 4, 5], [1, 3, 2, 2.5, 1, 2], [0, 0.5, 0, 0.2, 0, 0.1], step='pre', alpha=0.5, label='pre')
axs[0].fill_between([0, 1, 2, float('nan'), 4, 5, 6], [4, 5, 4, 5, 6, 5, 4], step='pre', y2=3.5, hatch='/')
axs[0].fill_between([7], [2], step='pre')
axs[0].plot([0, 1, 2, 3.5, 4, 5], [1, 3, 2, 2.5, 1, 2], drawstyle='steps-pre', color='k')
axs[0].legend()
axs[1].fill_between([0, 1, 2, 3.5, 4, 5], [1, 3, 2, 2.5, 1, 2], [0, 0.5, 0, 0.2, 0, 0.1], step='post', alpha=0.5, label='post')
axs[1].fill_between([0, 1, 2, float('nan'), 4, 5, 6], [4, 5, 4, 5, 6, 5, 4], step='post', y2=3.5, hatch='/')
axs[1].fill_between([7], [2], step='post')
axs[1].plot([0, 1, 2, 3.5, 4, 5], [1, 3, 2, 2.5, 1, 2], drawstyle='steps-post', color='k')
axs[1].legend()
axs[2].fill_between([0, 1, 2, 3.5, 4, 5], [1, 3, 2, 2.5, 1, 2], [0, 0.5, 0, 0.2, 0, 0.1], step='mid', alpha=0.5, label='mid')
axs[2].fill_between([0, 1, 2, float('nan'), 4, 5, 6], [4, 5, 4, 5, 6, 5, 4], step='mid', y2=3.5, hatch='/')
axs[2].fill_between([7], [2], step='mid')
axs[2].plot([0, 1, 2, 3.5, 4, 5], [1, 3, 2, 2.5, 1, 2], drawstyle='steps-mid', color='k')
axs[2].legend()

if __name__ == "__main__":
    plt.show()
