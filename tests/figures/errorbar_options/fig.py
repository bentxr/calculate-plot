# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.errorbar([0, 1, 2, 3], [1, 2.5, 2, 3], yerr=[[0.1, 0.2, 0.3, 0.4], [0.4, 0.3, 0.2, 0.1]], xerr=0.2, fmt='rs--', ecolor='k', elinewidth=0.5, capsize=4, capthick=2)
ax.errorbar([0.5, 1.5, 2.5], [0.5, 1, 0.7], yerr=0.3, fmt='none', barsabove=True, alpha=0.6, lw=2)
ax.errorbar([3.5], [1.5], xerr=[[0.3], [0.6]], marker='o', color='C4', label='single')

if __name__ == "__main__":
    plt.show()
