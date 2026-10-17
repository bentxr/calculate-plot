# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2)
axs[0, 0].plot([0, 1, 2], [0, 100000, 200000])
axs[0, 0].yaxis.tick_right()
axs[0, 0].yaxis.set_label_position('right')
axs[0, 0].set_ylabel('right label')
axs[0, 1].plot([0, 100000, 200000], [0, 1, 2])
axs[0, 1].xaxis.tick_top()
axs[0, 1].xaxis.set_label_position('top')
axs[0, 1].set_xlabel('top label')
axs[1, 0].yaxis.set_ticks_position('both')
axs[1, 0].xaxis.set_ticks_position('none')
axs[1, 1].yaxis.set_offset_position('right')
axs[1, 1].plot([0, 1], [1000000, 3000000])
axs[1, 1].xaxis.set_ticks_position('default')

if __name__ == "__main__":
    plt.show()
