# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.axhline(1.0, c='blue', color='red', lw=3.0, linewidth=1.0)
ax.axvline(0.5, ls='--', linestyle=':', color='green', marker='o', ms=9.0, markersize=3.0, mfc='yellow', markerfacecolor='black', mec='red', markeredgecolor='black', mew=2.0, markeredgewidth=0.5, aa=False, antialiased=True)
ax.axhline(2.0, alpha=0.5, color='C1', zorder=5, c='C2')
ax.axhline(3.0, marker='s', ms=12.0, linestyle='-.', ls=':')

if __name__ == "__main__":
    plt.show()
