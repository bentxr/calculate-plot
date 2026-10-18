# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(7.0, 2.5))
axs[0].pcolor([[0.0, 1.0, 2.0, 3.0, 4.0, 5.0], [0.0, 0.5403, 1.0806, 1.6209, 2.1612, 2.7015], [-0.0, -0.4161, -0.8323, -1.2484, -1.6646, -2.0807], [-0.0, -0.99, -1.98, -2.97, -3.96, -4.95]])
axs[1].pcolor([0, 1, 3, 4], [0, 2, 3], [[float('nan'), 1, 2], [3, float('nan'), 5]], edgecolors='k', linewidth=1.5, cmap='cividis')
axs[2].pcolor([[0.0, 1.0, 2.0, 3.0, 4.0], [0.25, 1.25, 2.25, 3.25, 4.25], [0.5, 1.5, 2.5, 3.5, 4.5], [0.75, 1.75, 2.75, 3.75, 4.75], [1.0, 2.0, 3.0, 4.0, 5.0]], [[0.0, 0.1, 0.4, 0.9000000000000001, 1.6], [1.0, 1.1, 1.4, 1.9000000000000001, 2.6], [2.0, 2.1, 2.4, 2.9000000000000004, 3.6], [3.0, 3.1, 3.4, 3.9000000000000004, 4.6], [4.0, 4.1, 4.4, 4.9, 5.6]], [[0.0, -0.3333333333333333, -0.6666666666666666, -1.0], [0.3333333333333333, 0.0, -0.3333333333333333, -0.6666666666666666], [0.6666666666666666, 0.3333333333333333, 0.0, -0.3333333333333333], [1.0, 0.6666666666666666, 0.3333333333333333, 0.0]], shading='flat', alpha=0.7, antialiased=True, vmin=-1)

if __name__ == "__main__":
    plt.show()
