# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(8.0, 3.0))
axs[0].set_yscale('log')
axs[0].axhline(1e-07)
axs[0].plot([17.426956, 1234.5])
axs[0].axhline(3000, xmin=0.25, color='C2')
axs[1].set_xscale('log')
axs[1].plot([3, 70, 900], [1, 3, 2], marker='o')
axs[1].axvline(0.013)
axs[1].axvspan(2000, 51000, alpha=0.3)
axs[2].set_xscale('log')
axs[2].set_yscale('log')
axs[2].axhspan(0.7, 7.1e-05, color='C1', alpha=0.4)
axs[2].axvline(123.456)
axs[2].scatter([5, 50, 500], [0.003, 0.03, 0.3])

if __name__ == "__main__":
    plt.show()
