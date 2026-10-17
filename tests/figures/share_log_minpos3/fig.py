# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(3, 1, sharex=True, figsize=(4.0, 6.0))
axs[0].plot([0, 10, 100], [0, 1, 2])
axs[1].plot([-1, 0.5, 20], [0, 1, 2])
axs[2].scatter([0, 3, 40], [0, 1, 2])
axs[0].set_xscale('log')

if __name__ == "__main__":
    plt.show()
