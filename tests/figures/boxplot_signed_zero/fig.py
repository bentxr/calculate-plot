# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2)
axs[1, 0].boxplot([[2.9865865773750535, -3.091, 3.699, float('nan'), 2.381, -2.907, -0.484, 1e-07], [0.0, -0.0, -1.41]])

if __name__ == "__main__":
    plt.show()
