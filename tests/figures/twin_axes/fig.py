# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(9, 4))
axs[0].plot([1, 2, 3, 4], [1, 4, 9, 16], color='C0', label='squares')
axs[0].set_ylabel('squares')
right = axs[0].twinx()
right.plot([1, 2, 3, 4, 5], [100000, 230000, 10000, 500000, 120000], color='C1', label='big')
right.set_ylabel('big values')
right.legend(loc='upper left')
axs[1].semilogx([1, 10, 100], [3, 1, 2])
top = axs[1].twiny()
top.plot([0, 0.5, 1], [1, 3, 2], color='r')
top.set_xlabel('top axis')
axs[1].set_frame_on(False)
plt.tight_layout()

if __name__ == "__main__":
    plt.show()
