# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(8.0, 2.6))
a = axs[0].imshow([[1.0, 0.316228, 0.1, 0.031623, 0.01], [3.162278, 1.0, 0.316228, 0.1, 0.031623], [10.0, 3.162278, 1.0, 0.316228, 0.1], [31.622777, 10.0, 3.162278, 1.0, 0.316228], [100.0, 31.622777, 10.0, 3.162278, 1.0]], norm='log')
fig.colorbar(a, ax=axs[0])
b = axs[1].pcolormesh([[0, -3, -6, -9], [3, 0, -3, -6], [6, 3, 0, -3], [9, 6, 3, 0]], norm='symlog', cmap='RdBu')
fig.colorbar(b, ax=axs[1], orientation='horizontal')
c = axs[2].scatter([1, 2, 3, 4], [4, 1, 3, 2], c=[0.5, 2.5, 1.0, 4.0], cmap='cividis')
fig.colorbar(c, ax=axs[2], location='left')

if __name__ == "__main__":
    plt.show()
