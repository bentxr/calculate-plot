# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.2, 5.4))
axs[0, 0].step([1, 2, 3, 4], [1, 3, 2, 4], label='pre')
axs[0, 0].step([1, 2, 3, 4], [2, 4, 3, 5], 'r--', where='post', label='post')
axs[0, 0].step([1, 2, 3, 4], [0, 2, 1, 3], where='mid', marker='o', label='mid')
axs[0, 0].legend()
axs[0, 1].stairs([3, 1, 4, 1, 5], label='default')
axs[0, 1].stairs([2, float('nan'), 3, 2.5], [0, 1, 2, 4, 5], baseline=None, linewidth=2)
axs[0, 1].stairs([1, 2, 1], edges=[5, 6, 7.5, 8], fill=True, color='C3', alpha=0.5, hatch='//')
axs[0, 1].legend()
axs[1, 0].stairs([1, 3, 2], orientation='horizontal', baseline=-1, fill=True)
axs[1, 0].stairs([4, 5, 3], baseline=[1, 2, 0.5], ec='k', linestyle='--')
axs[1, 1].stairs([])
axs[1, 1].stairs([2, 3, 1], baseline=1.5, color='g', fill=1, edgecolor='k', linewidth=1)
axs[1, 1].stairs([1000.0, 1, 10], baseline=0.1)
axs[1, 1].set_yscale('log')

if __name__ == "__main__":
    plt.show()
