# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.2, 5.4))
axs[0, 0].bar([1, 2, 3, 4], [3, 7, 2, 5], label='first')
axs[0, 0].bar([1, 2, 3, 4], [1, 2, 1, 2], bottom=[3, 7, 2, 5], label='stacked', edgecolor='k', linewidth=1.5)
axs[0, 0].legend()
axs[0, 1].barh([0, 1, 2], [5, -2, 3], height=0.5, color=['C2', 'C3', 'C4'], hatch='//', align='edge')
axs[1, 0].bar([0, 1, 2, 3, 4], [10, 100, 1000, 30, 3], log=True, width=0.6, yerr=[2, 20, 200, 6, 1], capsize=3, alpha=0.7)
axs[1, 1].bar([-2, -1, 0, 1, 2], [-3, 1, 4, -1, 2], facecolor='none', edgecolor=['r', 'g', 'b', 'c', 'm'], label=['a', 'b', 'c', 'd', 'e'])
axs[1, 1].plot([-2, 2], [3, 3], label='line')
axs[1, 1].legend(ncols=2, fontsize=7)

if __name__ == "__main__":
    plt.show()
