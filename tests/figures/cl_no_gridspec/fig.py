# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(layout='constrained')
ax = fig.add_axes([0.2, 0.2, 0.6, 0.6])
ax.plot([0, 1], [1, 0])
ax.set_title('no gridspec')

if __name__ == "__main__":
    plt.show()
