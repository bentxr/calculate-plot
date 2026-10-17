# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.prop_cycle': "cycler('color', ['r', '0.4', '#00aa0080', (0, 0, 1), 'E24A33'])",
})
fig, axs = plt.subplots(1, 2, figsize=(6.0, 3.0))
axs[0].plot([0, 1], [0, 1], [0, 1], [1, 2], [0, 1], [2, 3], [0, 1], [3, 4])
axs[0].plot([0, 1], [4, 5])
axs[0].plot([0, 1], [5, 6])
axs[0].plot([0, 1], [6, 7], color='C1')
axs[0].plot([0, 1], [7, 8], 'k--')
axs[0].plot([0, 1], [8, 9])
axs[0].axhline(4.5, color='C3')
axs[0].set_title('C4', color='C4')
axs[1].scatter([1, 2, 3], [1, 2, 3])
axs[1].fill_between([1, 2, 3], [0, 1, 0], [1, 2, 1])
axs[1].scatter([1, 2, 3], [3, 2, 1])
axs[1].errorbar([1, 2, 3], [2, 2, 2], yerr=0.3, capsize=3)
axs[1].errorbar([1, 2, 3], [2.5, 2.5, 2.5], yerr=0.2, fmt='none')
axs[1].plot([1, 3], [0.5, 0.5])
axs[1].vlines([1.5, 2.5], 0, 3)

if __name__ == "__main__":
    plt.show()
