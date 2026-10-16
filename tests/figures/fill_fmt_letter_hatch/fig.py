# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots()
ax.fill([0, 1, 2], [0, 1, 0], 'k:', hatch='\\')
ax.fill([2, 3, 4], [0, 1, 0], 'r--', [4, 5, 6], [0, 1, 0], 'C1', hatch='O')
ax.plot([0, 6], [1.5, 1.5], 'none', alpha=0.5)
ax.plot([0, 6], [1.7, 1.7], 'g-.', marker='o')

if __name__ == "__main__":
    plt.show()
