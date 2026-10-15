# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, sharey=True)
axs[1, 0].set_yscale('log')
axs[1, 1].text(33.40047, -1, 'below the log domain')
axs[0, 1].set_xscale('log')
axs[0, 1].text(0, 0.5, 'at x = 0 on a log axis')
axs[0, 0].text(0.5, 2, 'inside')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
