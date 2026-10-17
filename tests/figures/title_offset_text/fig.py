# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.0, 5.0))
axs[0, 0].plot([0, 1], [0, 3000000])
axs[0, 0].set_title('Left title meets the offset', loc='left')
axs[0, 1].plot([0, 1], [0, 3000000])
axs[0, 1].set_title('short')
axs[1, 0].plot([0, 1], [0, 0.0003])
axs[1, 0].set_title('right', loc='right')
axs[1, 0].set_title('A centred title wide enough to reach the offset text')
axs[1, 1].plot([0, 1], [0, 3000000])
axs[1, 1].set_title('Padded down into it', loc='left', pad=-14)

if __name__ == "__main__":
    plt.show()
