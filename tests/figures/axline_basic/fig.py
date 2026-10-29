# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(9.0, 4.0))
axs[0].plot([0, 1, 2, 3], [1, 3, 2, 4])
axs[0].axline((0, 0), (1, 1.5), color='C1', linestyle='--')
axs[0].axline((1, 2), slope=-0.5, color='k', lw=2, label='slope')
axs[0].axline((2.5, 0), (2.5, 1), color='g')
axs[0].axline((0, 3), slope=0, color='0.5')
axs[0].legend()
axs[1].plot([1, 10, 100], [1, 5, 2])
axs[1].set_xscale('log')
axs[1].axline((1, 1), (100, 4), color='r')
plt.axline((10, 0), (20, 6), marker='o')

if __name__ == "__main__":
    plt.show()
