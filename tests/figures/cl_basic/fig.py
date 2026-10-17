# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, layout='constrained', figsize=(7.0, 5.0))
axs[0, 0].plot([0, 1, 2, 3], [1, 1, 3, 2])
axs[0, 1].plot([0, 1, 2, 3], [1, 100, 3, 2])
axs[1, 0].plot([0, 1, 2, 3], [1, 10000, 3, 2])
axs[1, 1].plot([0, 1, 2, 3], [1, 1000000, 3, 2])
axs[0, 0].set_title('a title')
axs[0, 1].set_ylabel('a long y label')
axs[1, 0].set_xlabel('x label')
axs[1, 1].set_title('Right title', loc='right', fontsize=16)
fig.suptitle('Constrained')

if __name__ == "__main__":
    plt.show()
