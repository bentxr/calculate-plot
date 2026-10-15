# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 1)
axs[0].bar([1e-07], [18.8], color='C0', hatch='*')
axs[0].fill_between([1e-07, 0.3], [30.0, 10], hatch='*')
axs[1].bar([1, 2], [3, 4], color=[0.2, 0.4, 0.6, 1], hatch='x')
axs[1].fill_between([0.5, 2.5], [1, 2], hatch='x', facecolor=[0.2, 0.4, 0.6])

if __name__ == "__main__":
    plt.show()
