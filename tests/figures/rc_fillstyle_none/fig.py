# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'markers.fillstyle': 'none',
})
fig, axs = plt.subplots(1, 2, figsize=(8.0, 3.5))
axs[0].scatter([1, 2, 3, 4], [1, 3, 2, 4], c=[0.1, 0.4, 0.6, 0.9], s=120, label='mapped')
axs[0].scatter([1, 2, 3, 4], [2, 4, 3, 5], c=['red', 'C2', 'k', '0.5'], marker='s', label='listed')
axs[0].scatter([1, 2, 3, 4], [3, 5, 4, 6], marker='^', label='plain', linewidths=3)
axs[0].scatter([1, 2, 3, 4], [4, 6, 5, 7], c=[1, 2, 3, 4], marker='x', label='crosses')
axs[0].scatter([1, 2, 3, 4], [5, 7, 6, 8], c=[1, 2, 3, 4], edgecolors='red', alpha=0.5, vmin=0, vmax=10)
axs[0].legend()
axs[1].plot([1, 2, 3, 4], [1, 3, 2, 4], 'o-', label='line', markersize=10)
axs[1].plot([1, 2, 3, 4], [2, 4, 3, 5], marker='D', markerfacecolor='red', linestyle='none', label='diamonds')
axs[1].errorbar([1, 2, 3, 4], [3, 5, 4, 6], yerr=0.4, fmt='s', label='bars')
axs[1].legend()

if __name__ == "__main__":
    plt.show()
