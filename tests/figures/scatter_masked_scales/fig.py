# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt
import numpy as np

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 3, figsize=(10.0, 6.0))
axs[0, 0].scatter([0.0, -0.0, -1.0, 1e-300, 0.25, 0.5, 1.0, 2.0, 1e+300, -1e+300], [0.1, 0.2, 0.30000000000000004, 0.4, 0.5, 0.6000000000000001, 0.7000000000000001, 0.8, 0.9, 1.0])
axs[0, 0].set_xscale('logit')
axs[0, 0].set_xlim(0.2, 0.8)
axs[0, 1].scatter([0.0, -0.0, -1.0, 1e-300, 0.25, 0.5, 1.0, 2.0, 1e+300, -1e+300], [0.1, 0.2, 0.30000000000000004, 0.4, 0.5, 0.6000000000000001, 0.7000000000000001, 0.8, 0.9, 1.0])
axs[0, 1].set_xscale('logit', nonpositive='clip')
axs[0, 1].set_xlim(0.2, 0.8)
axs[0, 2].scatter([0.0, -0.0, -1.0, 1e-300, 0.25, 0.5, 1.0, 2.0, 1e+300, -1e+300], [0.1, 0.2, 0.30000000000000004, 0.4, 0.5, 0.6000000000000001, 0.7000000000000001, 0.8, 0.9, 1.0])
axs[0, 2].set_xscale('log', nonpositive='mask')
axs[0, 2].set_xlim(0.2, 0.8)
axs[1, 0].scatter([0.0, -0.0, -1.0, 1e-300, 0.25, 0.5, 1.0, 2.0, 1e+300, -1e+300], [0.1, 0.2, 0.30000000000000004, 0.4, 0.5, 0.6000000000000001, 0.7000000000000001, 0.8, 0.9, 1.0])
axs[1, 0].set_xscale('function', functions=(lambda x: 1 / x, lambda x: 1 / x))
axs[1, 0].set_xlim(0.2, 0.8)
axs[1, 1].scatter([0.0, -0.0, -1.0, 1e-300, 0.25, 0.5, 1.0, 2.0, 1e+300, -1e+300], [0.1, 0.2, 0.30000000000000004, 0.4, 0.5, 0.6000000000000001, 0.7000000000000001, 0.8, 0.9, 1.0])
axs[1, 1].set_xscale('function', functions=(lambda x: np.log10(x + 2), lambda x: 10 ** x - 2))
axs[1, 1].set_xlim(0.2, 0.8)
axs[1, 2].scatter([0.0, -0.0, -1.0, 1e-300, 0.25, 0.5, 1.0, 2.0, 1e+300, -1e+300], [0.1, 0.2, 0.30000000000000004, 0.4, 0.5, 0.6000000000000001, 0.7000000000000001, 0.8, 0.9, 1.0])
axs[1, 2].set_xscale('function', functions=(lambda x: x ** 2, lambda x: np.sqrt(x)))
axs[1, 2].set_xlim(0.2, 0.8)

if __name__ == "__main__":
    plt.show()
