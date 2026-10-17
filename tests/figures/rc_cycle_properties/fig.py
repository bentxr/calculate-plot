# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.prop_cycle': "cycler(color='rgb') + cycler(lw=[1, 2.5, 4]) + cycler(ls=['-', '--', ':']) + cycler(marker=['o', 's', '^'], ms=[4, 7, 10], mew=[0.5, 1.5, 2.5], mec='kkb', mfc=['w', 'y', 'none'], alpha=[1, 0.6, 0.3])",
})
fig, axs = plt.subplots(1, 2, figsize=(6.0, 3.0))
axs[0].plot([0, 1, 2], [0, 1, 0])
axs[0].plot([0, 1, 2], [1, 2, 1], color='k', linewidth=1, linestyle='-', marker='x', markersize=3, markeredgewidth=1, markeredgecolor='k', markerfacecolor='k', alpha=1)
axs[0].plot([0, 1, 2], [2, 3, 2])
axs[0].plot([0, 1, 2], [3, 4, 3], lw=6, marker=None)
axs[0].plot([0, 1, 2], [4, 5, 4], 'k+-.')
axs[0].plot([0, 1, 2], [5, 6, 5])
axs[1].errorbar([1, 2, 3], [1, 1, 1], yerr=0.3, capsize=3)
axs[1].errorbar([1, 2, 3], [2, 2, 2], yerr=0.3, xerr=0.2, capsize=4, fmt='none')
axs[1].errorbar([1, 2, 3], [3, 3, 3], yerr=0.3, fmt='D', color='m', alpha=0.9)
axs[1].scatter([1, 2, 3], [4, 4, 4])
axs[1].fill_between([1, 2, 3], [4.5, 5, 4.5], [5, 5.5, 5])

if __name__ == "__main__":
    plt.show()
