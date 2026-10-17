# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.scatter([1, 2, 3], [1, 3, 2], marker='s', s=-1)
ax.scatter([1, 2, 3], [2, 4, 3], marker='^', s=float('nan'))
ax.scatter([1, 2, 3], [3, 5, 4], marker='D', s=float('inf'))
ax.scatter([1, 2, 3], [4, 6, 5], marker='o', s=[-1, 40, 90])
ax.scatter([1, 2, 3], [5, 7, 6], marker='x', s=-4)
ax.scatter([1, 2, 3], [0, 2, 1], marker='s', s=0)

if __name__ == "__main__":
    plt.show()
