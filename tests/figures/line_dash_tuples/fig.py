# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots()
ax.plot([0, 1, 2, 3], [0, 0.5, 0, 0.5], linestyle=(0, (4, 2)), linewidth=1, label='0')
ax.plot([0, 1, 2, 3], [1, 1.5, 1, 1.5], linestyle=(2, (1, 1, 3, 1)), linewidth=2, label='1')
ax.plot([0, 1, 2, 3], [2, 2.5, 2, 2.5], linestyle=(-3, (5, 2)), linewidth=3, label='2')
ax.plot([0, 1, 2, 3], [3, 3.5, 3, 3.5], linestyle=(10.5, [2, 1]), linewidth=1, label='3')
ax.plot([0, 1, 2, 3], [4, 4.5, 4, 4.5], linestyle=(1, None), linewidth=2, label='4')
ax.plot([0, 1, 2, 3], [5, 5.5, 5, 5.5], linestyle=(0, (0, 3)), linewidth=3, label='5')
ax.plot([0, 1, 2, 3], [6, 6.5, 6, 6.5], linestyle=(7, (2.5,)), linewidth=1, label='6')
ax.axhline(7.5, ls=(0, (8, 4, 2, 4)))
ax.legend(loc='upper left', fontsize=7)

if __name__ == "__main__":
    plt.show()
