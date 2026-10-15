# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(7.2, 3.6))
axs[0].axhspan(0, 0.8, hatch='/', label='/')
axs[0].axhspan(1, 1.8, hatch='\\\\', label='\\\\', alpha=0.4)
axs[0].axhspan(2, 2.8, hatch='|', label='|', edgecolor='C3')
axs[0].axhspan(3, 3.8, hatch='-', label='-', hatchcolor='C4')
axs[0].axhspan(4, 4.8, hatch='+', label='+', edgecolor='none')
axs[0].axhspan(5, 5.8, hatch='x', label='x', fill=False)
axs[0].axhspan(6, 6.8, hatch='o', label='o', facecolor='0.9')
axs[0].legend(fontsize=7, loc='center right')
axs[1].axvspan(0, 0.7, hatch='O')
axs[1].axvspan(1, 1.7, hatch='.', hatchcolor='edge', edgecolor='C1')
axs[1].axvspan(2, 2.7, hatch='*', color='C2')
axs[1].axvspan(3, 3.7, hatch='//')
axs[1].axvspan(4, 4.7, hatch='xx|')
axs[1].axvspan(5, 5.7, hatch='*o-')
axs[1].axvspan(6.5, 7, hatch='', color='C5')
axs[1].axvspan(7.5, 8, hatch='/a', color='C6')

if __name__ == "__main__":
    plt.show()
