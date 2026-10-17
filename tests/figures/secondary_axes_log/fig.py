# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(9.0, 4.0))
axs[0].loglog([1, 10, 100, 1000], [2, 30, 200, 5000])
axs[0].secondary_xaxis('top', functions=(lambda x: x / 10, lambda x: x * 10))
axs[0].secondary_yaxis('right', functions=(lambda x: x * 3, lambda x: x / 3))
axs[1].plot([1, 5, 50, 500], [1, 2, 3, 4])
low = axs[1].secondary_xaxis('bottom', functions=(lambda x: x * 2, lambda x: x / 2))
axs[1].set_xscale('log')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
