# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2)
axs[0].plot([0, 1], [0, 1])
axs[0].annotate('far to the left of the axes', [0, 0.5], xytext=[-60, 0], textcoords='offset points', ha='right')
axs[1].plot([0, 1], [0, 1])
axs[1].annotate('below', [0.5, 0], xycoords='axes fraction', xytext=[0, -40], textcoords='offset points')
axs[1].annotate('clipped away', [5, 5], xytext=[0, 0.5], textcoords='axes fraction')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
