# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(8.0, 3.5))
axs[0].plot([0, 1, 2], [0, 1, 4], label='a curve')
axs[0].text(0.05, 3.7, 'a text where the legend would go')
axs[0].legend()
axs[1].plot([0, 1, 2], [4, 1, 0], label='another')
axs[1].text(1.0, 3.5, 'text', fontsize=30)
axs[1].axhspan(3.0, 4.0, alpha=0.2)
axs[1].legend(loc='best')

if __name__ == "__main__":
    plt.show()
