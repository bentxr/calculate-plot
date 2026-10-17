# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots()
ax.plot([0, 1, 2], [0, 1, 0], 'none', alpha=1)
ax.plot([0, 1, 2], [1, 2, 1], 'None', alpha=0.5, marker='o')
ax.plot([0, 1, 2], [2, 3, 2], 'none')
ax.plot([0, 1, 2], [3, 4, 3], 'NONE', alpha=0.25, linewidth=4)

if __name__ == "__main__":
    plt.show()
