# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(6.0, 4.0))
ax.plot([0, 0.5], [0, 0.3], marker='.', linestyle='', markersize=6)
ax.plot([1, 1.5], [0, 0.3], marker=',', linestyle='', markersize=8, markerfacecolor='none')
ax.plot([2, 2.5], [0, 0.3], marker='o', linestyle='', markersize=10, alpha=0.6)
ax.plot([3, 3.5], [0, 0.3], marker='v', linestyle='', markersize=12, markeredgecolor=(0.2, 0.4, 0.6))
ax.plot([4, 4.5], [0, 0.3], marker='^', linestyle='', markersize=6, markerfacecolor='none', markeredgewidth=2.5)
ax.plot([5, 5.5], [0, 0.3], marker='<', linestyle='', markersize=8)
ax.plot([6, 6.5], [0, 0.3], marker='>', linestyle='', markersize=10)
ax.plot([7, 7.5], [0, 0.3], marker='1', linestyle='', markersize=12, markerfacecolor='none', alpha=0.6, markeredgecolor=(0.2, 0.4, 0.6))
ax.plot([8, 8.5], [0, 0.3], marker='2', linestyle='', markersize=6)
ax.plot([0, 0.5], [1, 1.3], marker='3', linestyle='', markersize=8)
ax.plot([1, 1.5], [1, 1.3], marker='4', linestyle='', markersize=10, markerfacecolor='none')
ax.plot([2, 2.5], [1, 1.3], marker='8', linestyle='', markersize=12, markeredgecolor=(0.2, 0.4, 0.6), markeredgewidth=2.5)
ax.plot([3, 3.5], [1, 1.3], marker='s', linestyle='', markersize=6, alpha=0.6)
ax.plot([4, 4.5], [1, 1.3], marker='p', linestyle='', markersize=8, markerfacecolor='none')
ax.plot([5, 5.5], [1, 1.3], marker='P', linestyle='', markersize=10)
ax.plot([6, 6.5], [1, 1.3], marker='*', linestyle='', markersize=12, markeredgecolor=(0.2, 0.4, 0.6))
ax.plot([7, 7.5], [1, 1.3], marker='h', linestyle='', markersize=6, markerfacecolor='none')
ax.plot([8, 8.5], [1, 1.3], marker='H', linestyle='', markersize=8, alpha=0.6)
ax.plot([0, 0.5], [2, 2.3], marker='+', linestyle='', markersize=10, markeredgewidth=2.5)
ax.plot([1, 1.5], [2, 2.3], marker='x', linestyle='', markersize=12, markerfacecolor='none', markeredgecolor=(0.2, 0.4, 0.6))
ax.plot([2, 2.5], [2, 2.3], marker='X', linestyle='', markersize=6)
ax.plot([3, 3.5], [2, 2.3], marker='D', linestyle='', markersize=8)
ax.plot([4, 4.5], [2, 2.3], marker='d', linestyle='', markersize=10, markerfacecolor='none', alpha=0.6)
ax.plot([5, 5.5], [2, 2.3], marker='|', linestyle='', markersize=12, markeredgecolor=(0.2, 0.4, 0.6))
ax.plot([6, 6.5], [2, 2.3], marker='_', linestyle='', markersize=6)
ax.plot([0, 8], [3.2, 3.2], color=(0.9, 0.1, 0.1, 0.5), marker='o', fillstyle='none', ls=':')

if __name__ == "__main__":
    plt.show()
