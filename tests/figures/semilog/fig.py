# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2)
axs[0].semilogx([1, 10, 100, 1000], [1, 2, 3, 4], 'o-', label='semilogx')
axs[1].semilogy([1, 2, 3, 4], [1, 10, 100, 1000], color='C2')
axs[0].legend()

if __name__ == "__main__":
    plt.show()
