# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots()
ax.vlines([1, 2, 3], 0, [1, 2, 3], linestyles='None', colors='C1', linewidths=3, label='v')
ax.hlines([0.5, 1.5], 0, 4, linestyles=['none', ' '])
ax.hlines([2.5], 0, 4, linestyles='', colors='k')
ax.legend()

if __name__ == "__main__":
    plt.show()
