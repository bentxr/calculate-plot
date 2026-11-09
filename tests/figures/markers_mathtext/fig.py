# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(7.0, 3.0))
axs[0].plot([0, 1, 2, 3], [1, 3, 2, 4], marker='$\\alpha$', markersize=14, label='alpha')
axs[0].plot([0, 1, 2, 3], [2, 1, 3, 2], marker='$x^2$', markersize=12, markerfacecolor='C3', markeredgecolor='k', markeredgewidth=0.5, linestyle='none', label='squared')
axs[0].plot([0.5, 1.5, 2.5], [3.5, 0.5, 1], marker='$\\frac{1}{2}$', markersize=16, linestyle=':', label='half')
axs[0].legend()
axs[1].scatter([0, 1, 2, 3], [0, 1, 4, 9], marker='$\\clubsuit$', s=[50, 100, 200, 400], c=[1, 2, 3, 4])
axs[1].plot([0, 3], [9, 0], marker='$1$', markersize=10)

if __name__ == "__main__":
    plt.show()
