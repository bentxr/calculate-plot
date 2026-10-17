# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.2, 4.8))
axs[0, 0].plot([0, 1, 2], [0, 1, 4])
axs[0, 0].set_aspect('equal')
axs[0, 1].plot([0, 10], [0, 1])
axs[0, 1].set_aspect(2, anchor='SW')
axs[0, 1].set_title('aspect 2, SW')
axs[1, 0].plot([1, 10, 100], [1, 5, 2])
axs[1, 0].set_xscale('log')
axs[1, 0].set(aspect=0.5)
axs[1, 1].plot([0, 1], [0, 3])
axs[1, 1].set_aspect('auto')
axs[1, 1].set_aspect(1, anchor=(0.2, 0.8))
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
