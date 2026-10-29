# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(6.4, 4.0))
ax.broken_barh([[1, 2], [5, 1.5]], [0, 1])
ax.broken_barh([[0.5, 1], [3, 2], [7, 0.5]], [2.5, 1.5], facecolors=['C1', 'C2', 'C3'], edgecolors='k', linewidths=2, align='center')
ax.broken_barh([[2, 3]], [6, 1], color='r', alpha=0.5, hatch='//', align='top', label='top')
plt.broken_barh([[8, 1.5]], [1, 4], facecolor='0.7', zorder=0)
ax.legend()

if __name__ == "__main__":
    plt.show()
