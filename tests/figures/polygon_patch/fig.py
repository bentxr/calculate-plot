# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(7.2, 2.6))
tri = plt.Polygon([[0, 0], [1, 0.5], [0.5, 1]], closed=True, fc='C2', ec='k', lw=2)
axs[0].add_patch(tri)
open = plt.Polygon([[0.1, 0.1], [0.9, 0.2], [0.8, 0.9], [0.1, 0.1]], closed=False, fill=False, edgecolor='C3', hatch='o')
axs[0].add_patch(open)
big = plt.Polygon([[-1, -1], [3, 0], [2, 4]], label='big', alpha=0.3, color='C4', facecolor='k')
axs[1].add_patch(big)
axs[1].plot([0, 1], [0, 1])
axs[1].legend()
late = plt.Polygon([[2, 2], [3, 5], [4, 2]], color='C1', edgecolor='nonsense', fc='C0')
axs[2].add_patch(late)
axs[2].set_xlim(0, 6)
axs[2].set_ylim(0, 6)

if __name__ == "__main__":
    plt.show()
