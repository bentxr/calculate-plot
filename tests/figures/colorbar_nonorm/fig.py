# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt
from matplotlib import colors

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

nn = colors.NoNorm()
fig, ax = plt.subplots(figsize=(4.0, 3.0))
a = ax.imshow([[0, 1, 2, 3, 4], [5, 6, 7, 8, 9], [0, 1, 2, 3, 4], [5, 6, 7, 8, 9]], norm=nn, cmap='tab10')
fig.colorbar(a)

if __name__ == "__main__":
    plt.show()
