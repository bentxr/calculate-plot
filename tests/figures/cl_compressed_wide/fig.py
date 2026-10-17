# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 3, layout='compressed', figsize=(8.0, 3.0))
axs[0, 0].plot([0, 1, 2], [0, 2, 1])
axs[0, 0].set_aspect('equal')
axs[0, 1].plot([0, 4], [0, 1])
axs[0, 1].set_aspect('equal')
axs[0, 2].plot([0, 1], [0, 1])
axs[0, 2].set_aspect('equal')
axs[1, 0].plot([0, 1], [0, 1])
axs[1, 0].set_aspect('equal')
axs[1, 1].plot([0, 1], [0, 3])
axs[1, 1].set_aspect('equal')
axs[1, 2].plot([0, 2], [0, 1])
axs[1, 2].set_aspect('equal')
fig.suptitle('compressed, wider than tall')

if __name__ == "__main__":
    plt.show()
