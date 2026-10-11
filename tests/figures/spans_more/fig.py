# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.vlines([0, 1, 2, 3], 0, [1, 3, 2, 4])
ax.axhline(6, lw=3, alpha=0.5)
ax.axvline(-1)
ax.hlines([0.5, 1.5], [0, 1], 3, linestyles='dashed', linewidths=[1, 2], colors=['C1', 'C2'])
ax.axvspan(1.2, 1.8, ymin=0.1, ymax=0.5, facecolor='yellow', edgecolor='k', lw=2)
ax.axhspan(4.5, 5, fill=False, hatch=None, ls='--', ec='C3')

if __name__ == "__main__":
    plt.show()
