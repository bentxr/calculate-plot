# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2)
axs[0].scatter([float('-inf'), 2], [4, 0], c=[-4.388, 0.5], s=200)
axs[1].scatter([-1.666, -4.4, 1], [4, float('inf'), 2], c=[2.136, 3.426, 2.136], s=200)

if __name__ == "__main__":
    plt.show()
