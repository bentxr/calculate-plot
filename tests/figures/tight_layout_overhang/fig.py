# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(4.0, 2.5))
axs[0].plot([0, 1], [0, 1], label='a rather long legend label')
axs[0].legend(loc='upper left')
axs[1].set_title('A very long title for a narrow Axes')
axs[1].set_ylabel('a very long y label, much longer than the short Axes is tall')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
