# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'hatch.linewidth': 1.5,
})
fig, axs = plt.subplots(1, 2, figsize=(6.0, 3.0))
axs[0].fill([0, 1, 1, 0], [0, 0, 1, 1], facecolor='C1', edgecolor='k', hatch='//')
axs[0].fill([1.5, 2.5, 2.0], [0, 0, 1], facecolor='0.8', edgecolor='C0', hatch='x')
axs[0].fill([0, 1, 1, 0], [1.5, 1.5, 2.5, 2.5], fill=False, edgecolor='C2', hatch='o.')
axs[0].fill([1.5, 2.5, 2.5, 1.5], [1.5, 1.5, 2.5, 2.5], facecolor='C1', edgecolor='k', hatch='//')
axs[0].fill([2.6, 2.9, 2.9, 2.6], [0, 0, 2.5, 2.5], facecolor='C3', edgecolor='C4', hatch='-|', alpha=0.5)
axs[0].set_xlim(0, 3)
axs[0].set_ylim(0, 3)
axs[0].axis('off')
axs[1].fill_between([0, 1, 2, 3], [1, 2, 1, 2], [0, 0.5, 0, 0.5], facecolor='C5', edgecolor='C6', hatch='\\\\')
axs[1].fill_between([0, 1, 2, 3], [3, 3.5, 3, 3.5], [2.2, 2.7, 2.2, 2.7], facecolor='none', edgecolor='C7', hatch='+')
axs[1].axis('off')

if __name__ == "__main__":
    plt.show()
