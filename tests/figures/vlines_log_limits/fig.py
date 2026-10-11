# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(8.0, 6.0))
axs[0, 0].set_yscale('log')
axs[0, 0].vlines([1.0, 2.0, 3.0], 10.0, 1000.0)
axs[0, 1].set_xscale('log')
axs[0, 1].hlines([1.0, 2.0, 3.0], 0.3, 7000.0, colors='C1')
axs[0, 1].vlines([10.0, 100.0, -10.0, 0.0], 1.5, 2.5)
axs[1, 0].set_yscale('log')
axs[1, 0].set_xscale('log')
axs[1, 0].vlines([3.0, 30.0, 0.0, -5.0], [-1.0, 0.0, 2.0, 5.0], [10.0, 100.0, 50.0, 70.0])
axs[1, 0].errorbar([2.0, 20.0, 200.0], [5.0, 20.0, 60.0], yerr=[1.0, 30.0, 10.0], xerr=[0.5, 5.0, 300.0], fmt='o')
axs[1, 1].vlines([1.0, 2.0, 3.0], 10.0, 1000.0)
axs[1, 1].hlines([100.0, 5.0], 1.5, 4.5)
axs[1, 1].set_yscale('log')

if __name__ == "__main__":
    plt.show()
