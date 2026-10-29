# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 3, figsize=(11.0, 7.0))
axs[0, 0].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[0, 0].set_aspect('equal', adjustable='datalim')
axs[0, 1].scatter([0.3, 1.7, 2.2, 4.9, 6.1], [11.0, -4.0, 28.0, 9.0, 37.0])
axs[0, 1].set_xlim(0, 8)
axs[0, 1].set_aspect(0.2, 'datalim')
axs[0, 2].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1, 10, 300, 40, 5])
axs[0, 2].set_yscale('log')
axs[0, 2].set_aspect(3, adjustable='datalim')
axs[1, 0].set_aspect(2)
axs[1, 0].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
t1 = axs[1, 0].twinx()
t1.plot([0.3, 1.7, 2.2, 4.9, 6.1], [3.3000000000000003, -1.2000000000000002, 8.399999999999999, 2.7, 11.100000000000001], color='C1')
t2 = axs[1, 1].twinx()
axs[1, 1].pie([3, 2, 5])
axs[1, 2].plot([1.1, -0.4, 2.8, 0.9, 3.7], [0.3, 1.7, 2.2, 4.9, 6.1])
axs[1, 2].set_ylim(7, -1)
axs[1, 2].set_adjustable('datalim')
axs[1, 2].set_aspect(0.5, anchor='NW')

if __name__ == "__main__":
    plt.show()
