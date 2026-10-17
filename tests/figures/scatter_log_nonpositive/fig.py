# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots()
ax.scatter([0, 3, 40, -2], [5, 0, 2, 7])
ax.scatter([1, -1, 20, 0], [3, 4, -5, 0], s=[10, 40, 90, 160])
ax.set_xscale('log')
ax.set_yscale('log')

if __name__ == "__main__":
    plt.show()
