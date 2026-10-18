# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(5.0, 2.5))
axs[0].set_xscale('log')
axs[0].imshow([[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12]], extent=[1, 1000, 0, 3], aspect='auto', interpolation='bilinear')
axs[1].set_yscale('log')
axs[1].imshow([[0.5, 0.25], [0.125, 1.0]], extent=[0, 2, 1, 100], aspect='auto')

if __name__ == "__main__":
    plt.show()
