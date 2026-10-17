# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 0.5, 1.5, 2.25, 3], [2, -1, 0.5, 3.25, 1], 'ro--')
ax.plot([0, 1, 2, 3], [0.1, 0.7, 2.2, -0.4], 'g^:', linewidth=2, markersize=8)
ax.plot([1.5, 0.25, 2.75])
ax.plot([0.5, 1, 2], [3, 2.5, 2.9], color='C5', linestyle='-.', marker='s', markerfacecolor='none')
ax.plot([2.5, 2.8], [-0.8, 0.3], color='#1f77b480', lw=3, solid_capstyle='round')

if __name__ == "__main__":
    plt.show()
