# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(9, 4))
axs[0].plot([0, 1, 2, 3, 4, 5], [0, 1, 4, 9, 16, 25], marker='o')
axs[0].set_title('main')
ins = axs[0].inset_axes([0.1, 0.55, 0.4, 0.35])
ins.plot([2, 2.5, 3], [4, 6.25, 9], color='r')
ins.set_title('zoom', fontsize=8)
axs[1].bar([1, 2, 3], [3, 5, 2])
ins2 = axs[1].inset_axes([0.6, 0.6, 0.35, 0.35], zorder=1)
ins2.semilogy([1, 2, 3], [10, 1000, 100])
out = axs[1].inset_axes([0.85, -0.3, 0.3, 0.25])
out.set_xlabel('outside')
plt.tight_layout()

if __name__ == "__main__":
    plt.show()
