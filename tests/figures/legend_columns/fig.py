# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(6.0, 2.5))
axs[0].plot([1, 2, 3], [0, 2, 1], label='one')
axs[0].plot([1, 2, 3], [1, 0, 2], label='two')
axs[0].plot([1, 2, 3], [2, 1, 0], label='three')
axs[0].legend(ncols=1, ncol=2.9)
axs[1].plot([1, 2, 3], [0, 2, 1], label='one')
axs[1].plot([1, 2, 3], [1, 0, 2], label='two')
axs[1].legend(ncols=1000000, ncol=0)

if __name__ == "__main__":
    plt.show()
