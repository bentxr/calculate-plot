# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(6.0, 3.0))
axs[0].plot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 3, 2, 4, 3, 5, 4, 6, 5, 7], marker='o', markersize=8, markerfacecolor='C1', markeredgecolor='k')
axs[0].plot([0, 9], [8, 8], marker='s', linestyle='none', color='C2')
axs[0].plot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [0, 0.5, 0, 0.5, 0, 0.5, 0, 0.5, 0, 0.5], marker='^', fillstyle='none', markeredgewidth=2, linestyle='none', color='C3')
axs[0].plot([-5, 2, 20], [6, 6, 6], marker='D', linestyle='none', color='0.3')
axs[0].fill_between([0, 3, 6, 9], [1, 2, 1, 2], [0, 0.5, 0, 0.5], alpha=0.3, color='C4')
axs[0].vlines([1.5, 4.5, 7.5], 0, 8, linestyles='dashed', colors=['C5', 'C6', 'C7'])
axs[0].set_xlim(-1, 10)
axs[0].set_ylim(-1, 9)
axs[0].axis('off')
axs[1].scatter([0, 1, 2, 3, 4, 5, 6, 7], [3, 1, 4, 1, 5, 9, 2, 6], s=[20, 40, 60, 80, 100, 120, 140, 160], c=[0, 1, 2, 3, 4, 5, 6, 7], edgecolors='k')
axs[1].scatter([0, 2, 4, 6], [8, 8, 8, 8], c=[[1, 0, 0, 0.2], [0, 1, 0, 0.5], [0, 0, 1, 0.8], [0, 0, 0, 1]], marker='s', s=80)
axs[1].scatter([1, 3, 5, 7], [0, 0, 0, 0], color='C2', marker='x', s=50)
axs[1].scatter([0, 0.7, 1.4, 2.1, 2.8, 3.5, 4.2, 4.9, 5.6, 6.3, 7.0, 7.7, 30], [6.5, 7, 6.5, 7, 6.5, 7, 6.5, 7, 6.5, 7, 6.5, 7, 7], c=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12], s=30, cmap='viridis', linewidths=1.5, edgecolors='face')
axs[1].axis('off')

if __name__ == "__main__":
    plt.show()
