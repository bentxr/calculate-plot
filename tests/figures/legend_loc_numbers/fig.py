# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 3, figsize=(8.0, 5.0))
axs[0, 0].plot([0, 1, 2], [0, 1, 0], label='line', marker='o')
axs[0, 0].plot([0, 1, 2], [1, 0, 1], label='other')
axs[0, 0].legend(loc=0, fontsize=7)
axs[0, 1].plot([0, 1, 2], [1, 2, 1], label='line', marker='o')
axs[0, 1].plot([0, 1, 2], [2, 1, 2], label='other')
axs[0, 1].legend(loc=3, fontsize=7)
axs[0, 2].plot([0, 1, 2], [2, 3, 2], label='line', marker='o')
axs[0, 2].plot([0, 1, 2], [3, 2, 3], label='other')
axs[0, 2].legend(loc=10, fontsize=7)
axs[1, 0].plot([0, 1, 2], [3, 4, 3], label='line', marker='o')
axs[1, 0].plot([0, 1, 2], [4, 3, 4], label='other')
axs[1, 0].legend(loc=True, fontsize=7)
axs[1, 1].plot([0, 1, 2], [4, 5, 4], label='line', marker='o')
axs[1, 1].plot([0, 1, 2], [5, 4, 5], label='other')
axs[1, 1].legend(loc=(0.2, 0.6), fontsize=7)
axs[1, 2].plot([0, 1, 2], [5, 6, 5], label='line', marker='o')
axs[1, 2].plot([0, 1, 2], [6, 5, 6], label='other')
axs[1, 2].legend(loc=[1, 0], fontsize=7)

if __name__ == "__main__":
    plt.show()
