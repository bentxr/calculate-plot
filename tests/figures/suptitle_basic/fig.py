# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(5.0, 3.0))
axs[0].plot([0, 1], [1, 0])
axs[1].plot([0, 1], [0, 1])
fig.suptitle('Two panels')

if __name__ == "__main__":
    plt.show()
