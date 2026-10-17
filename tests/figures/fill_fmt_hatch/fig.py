# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots()
ax.fill([0, 1, 2], [0, 1, 0], 'r', hatch='/')
ax.fill([2, 3, 4], [0, 1, 0], 'C2', hatch='o')
ax.fill([4.5], 'k', hatch='|')
ax.fill([0, 2, 4], [1.5, 2, 1.5], hatch='x', facecolor='r')

if __name__ == "__main__":
    plt.show()
