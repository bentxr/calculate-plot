# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(6.0, 2.8))
axs[0].pcolormesh([[1.0, 0.87758, 0.5403, 0.07074, -0.41615, -0.80114, -0.98999], [1.84147, 1.71905, 1.38177, 0.91221, 0.42532, 0.04033, -0.14852], [1.9093, 1.78688, 1.4496, 0.98003, 0.49315, 0.10815, -0.0807], [1.14112, 1.0187, 0.68142, 0.21186, -0.27503, -0.66002, -0.84887], [0.2432, 0.12078, -0.2165, -0.68607, -1.17295, -1.55795, -1.74679]])
axs[1].pcolormesh([[0, 1, 2, 3], [4, 5, 6, 7], [8, 9, 10, 11], [12, 13, 14, 15]], cmap='magma', edgecolors='k', linewidth=0.5, vmin=2, vmax=12)

if __name__ == "__main__":
    plt.show()
