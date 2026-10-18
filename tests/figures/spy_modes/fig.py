# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(7.0, 2.6))
axs[0].spy([[-1, -1, -1, -1, -1, -1, -1, -1, -1, -1], [-1, 0, 1, -1, 0, 1, -1, 0, 1, -1], [-1, 1, 0, -1, 1, 0, -1, 1, 0, -1], [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1], [-1, 0, 1, -1, 0, 1, -1, 0, 1, -1], [-1, 1, 0, -1, 1, 0, -1, 1, 0, -1], [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1], [-1, 0, 1, -1, 0, 1, -1, 0, 1, -1]])
axs[1].spy([[0.0, -0.1, -0.2, -0.30000000000000004, -0.4, -0.5], [0.1, 0.0, -0.1, -0.2, -0.30000000000000004, -0.4], [0.2, 0.1, 0.0, -0.1, -0.2, -0.30000000000000004], [0.30000000000000004, 0.2, 0.1, 0.0, -0.1, -0.2], [0.4, 0.30000000000000004, 0.2, 0.1, 0.0, -0.1], [0.5, 0.4, 0.30000000000000004, 0.2, 0.1, 0.0]], precision=0.25, markersize=5)
axs[2].spy([[0, 1, 0, 1, 0, 1, 0], [1, 0, 1, 0, 1, 0, 1], [0, 1, 0, 1, 0, 1, 0], [1, 0, 1, 0, 1, 0, 1], [0, 1, 0, 1, 0, 1, 0]], marker='o', origin='lower', color='C2')

if __name__ == "__main__":
    plt.show()
