# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(5.0, 2.5))
axs[0].plot([1, 2, 3], [0, 2, 1])
axs[0].grid(c='#ff00ff80')
axs[1].plot([1, 2, 3], [0, 2, 1])
axs[1].grid(True, which='both', c='#0a08', lw=2.0, ls='--')

if __name__ == "__main__":
    plt.show()
