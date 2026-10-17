# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 3, figsize=(6.0, 4.0))
fig.subplots_adjust(0.2, None, 0.95, wspace=0.6, hspace=0.1)
axs[0, 0].plot([0, 1], [0, 1])

if __name__ == "__main__":
    plt.show()
