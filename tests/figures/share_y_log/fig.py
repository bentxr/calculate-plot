# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, sharey=True, figsize=(6.0, 3.0))
axs[0].plot([0, 1, 2], [1, 10, 300])
axs[1].plot([0, 1, 2], [0.02, 5, 70])
axs[1].set_yscale('log')

if __name__ == "__main__":
    plt.show()
