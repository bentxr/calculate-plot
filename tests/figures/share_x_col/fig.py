# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 1, sharex=True)
axs[0].plot([0, 1, 2], [0, 1, 4])
axs[1].plot([1, 5], [3, 4])
axs[0].set_xlabel('hidden by the grid')
axs[1].set_xlabel('shared x')

if __name__ == "__main__":
    plt.show()
