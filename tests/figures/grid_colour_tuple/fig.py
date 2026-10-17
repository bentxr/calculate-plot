# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2)
axs[0].plot([0, 1, 2], [2, 0, 1])
axs[0].grid(color=(0.9, 0.1, 0.4), linestyle=':')
axs[1].vlines([0.5, 1.5], 0, 1, alpha=None)
axs[1].hlines([0.25, 0.75], 0, 2, alpha=None, colors='C1')
axs[1].grid(c=[0.1, 0.2, 0.3, 0.5], axis='y')

if __name__ == "__main__":
    plt.show()
