# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, sharex=True)
axs[0].scatter([-1.58, -0.08, 1.42, 2.92, 4.42, 5.92, 7.42, 8.92, 10.42], [9.0, -3.301, 5.3, 8.44, 8.0, 4.125, -3.0, -2.0, -0.65])
axs[0].set_ylim(-4.2, 780.5)
axs[1].axvspan(-2.729, -0.944)

if __name__ == "__main__":
    plt.show()
