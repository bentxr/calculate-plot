# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(7.0, 3.0))
axs[0].plot([0, 1, 2], [1000000, 2000000, 3000000])
axs[1].plot([10000, 10001], [0, 1])
axs[1].set_xlabel('a long x label that is wide')
axs[1].set_title('Middle', loc='left')
axs[2].set_ylabel('right panel')
axs[2].set_title('Third')
fig.suptitle('Overall title')
fig.tight_layout(pad=0.5, w_pad=2.0, h_pad=0.3)

if __name__ == "__main__":
    plt.show()
