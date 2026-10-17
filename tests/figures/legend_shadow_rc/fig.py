# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'legend.shadow': True,
})
fig, axs = plt.subplots(2, 2, figsize=(6.4, 4.8))
axs[0, 0].plot([0, 1, 2], [0, 1, 0], label='line', marker='o')
axs[0, 0].fill_between([0, 1, 2], [0, 0.5, 0], label='band')
axs[0, 0].legend()
axs[0, 1].plot([0, 1, 2], [1, 2, 1], label='line', marker='o')
axs[0, 1].fill_between([0, 1, 2], [0, 0.5, 0], label='band')
axs[0, 1].legend(loc='lower right')
axs[1, 0].plot([0, 1, 2], [2, 3, 2], label='line', marker='o')
axs[1, 0].fill_between([0, 1, 2], [0, 0.5, 0], label='band')
axs[1, 0].legend(facecolor='yellow')
axs[1, 1].plot([0, 1, 2], [3, 4, 3], label='line', marker='o')
axs[1, 1].fill_between([0, 1, 2], [0, 0.5, 0], label='band')
axs[1, 1].legend(shadow=0)

if __name__ == "__main__":
    plt.show()
