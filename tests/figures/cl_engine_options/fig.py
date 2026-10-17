# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, constrained_layout={'h_pad': 0.3, 'w_pad': 0.0, 'wspace': 0.3, 'rect': (0.05, 0.0, 0.9, 0.95)}, figsize=(6.0, 3.0))
axs[0].plot([0, 1, 2, 3], [1, 1, 3, 2])
axs[1].plot([0, 1, 2, 3], [1, 1, 3, 2])
fig.subplots_adjust(left=0.3)

if __name__ == "__main__":
    plt.show()
