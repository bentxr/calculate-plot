# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(9.0, 3.0))
axs[0].plot([0, 1, 2], [0, 1, 0.5])
axs[0].set_xticks([0, 1, 2], labels=[1, 2.5, 'c'])
axs[0].set_yticks([0, 0.5, 1], ['lo', 'mid', 'hi'], rotation=45, color='C1', fontsize=8)
axs[1].plot([0, 1], [1, 100])
axs[1].set_yscale('log')
axs[1].set_xticks([0, 0.5, 1])
axs[1].set_xticks([0.25, 0.75], labels=['q1', 'q3'], minor=True, color='0.4', fontstyle='italic')
axs[1].set_yticks([2, 20, 50], minor=True)
axs[2].plot([0, 3], [0, 3])
axs[2].set_xticks([0, 1], labels=['a', 'b'], color='red', fontweight='bold')
axs[2].set_xticks([0, 1, 2, 3])
axs[2].set_yticks([0, 1.5, 3], labels=[True, None, -0.0])

if __name__ == "__main__":
    plt.show()
