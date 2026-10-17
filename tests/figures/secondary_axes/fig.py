# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt
import numpy as np

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(12.0, 4.0))
axs[0].plot([-10, 0, 10, 20, 30, 40], [3, 5, 4, 6, 8, 7])
axs[0].set_xlabel('Celsius')
axs[0].set_title('temperature')
secx = axs[0].secondary_xaxis('top', functions=(lambda x: x * 1.8 + 32, lambda x: (x - 32) / 1.8))
secx.set_xlabel('Fahrenheit')
axs[1].plot([0, 1, 2, 3, 4], [0, 45, 90, 135, 180])
secy = axs[1].secondary_yaxis('right', functions=(lambda x: np.deg2rad(x), lambda x: np.rad2deg(x)))
secy.set_ylabel('radians')
axs[2].plot([1, 2, 3, 4, 5], [1, 4, 9, 16, 25], marker='o')
axs[2].secondary_xaxis(0.5)
axs[2].secondary_yaxis('left', functions=(lambda x: x ** 3, lambda x: np.cbrt(x)))
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
