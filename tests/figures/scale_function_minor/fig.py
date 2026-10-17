# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt
import numpy as np

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'xtick.minor.visible': True,
    'ytick.minor.visible': True,
})
fig, axs = plt.subplots(1, 2, figsize=(8.0, 3.5))
axs[0].plot([0, 10, 20, 30, 40], [5, 3, 8, 1, 6])
axs[0].set_xscale('function', functions=(lambda x: np.deg2rad(x), lambda x: np.rad2deg(x)))
axs[0].grid(True)
axs[1].plot([1, 2, 3], [100, 1000, 10000])
axs[1].set_yscale('functionlog', functions=(lambda x: x, lambda x: x))
axs[1].set_ylim(50, 50000)
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
