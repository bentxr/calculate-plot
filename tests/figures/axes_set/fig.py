# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2)
axs[0].plot([1, 10, 100], [1, 5, 2])
axs[0].set(xscale='log', xlim=(0.5, 200), ylim=[0, 6], title='set', xlabel='x', ylabel='y')
axs[1].plot([0, 1, 2], [2, 1, 2])
axs[1].set(ylim=(3, 0), title='reversed')

if __name__ == "__main__":
    plt.show()
