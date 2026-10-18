# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(5.0, 4.0))
im = axs[0, 0].imshow([[0, 1, 2], [1, 2, 3], [2, 3, 4]])
fig.colorbar(im, ax=axs, shrink=0.6)
plt.sca(axs[1, 1])
plt.imshow([[0, -1, -2], [1, 0, -1], [2, 1, 0]])
plt.colorbar(location='top', fraction=0.05)

if __name__ == "__main__":
    plt.show()
