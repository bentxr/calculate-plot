# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'legend.numpoints': 3,
    'legend.scatterpoints': 5,
    'legend.markerscale': 1.5,
})
fig, axs = plt.subplots(1, 2, figsize=(7.0, 3.5))
axs[0].plot([0, 1, 2], [0, 1, 0], 'o-', label='a line')
axs[0].plot([0, 1, 2], [1, 2, 1], label='no marker', linestyle='--')
axs[0].scatter([0, 1, 2], [2, 3, 2], s=[20, 80, 300], label='sizes')
axs[0].vlines([0.5, 1.5], 0, 3, label='lines', linestyles='dotted')
axs[0].legend(loc='upper left')
axs[1].errorbar([0, 1, 2], [0, 1, 0], yerr=0.2, xerr=0.1, capsize=3, fmt='s-', label='both')
axs[1].errorbar([0, 1, 2], [1, 2, 1], yerr=0.2, label='no caps')
axs[1].errorbar([0, 1, 2], [2, 3, 2], xerr=0.2, fmt='none', capsize=4, label='no line')
axs[1].scatter([0, 1, 2], [3, 4, 3], label='one size')
axs[1].legend(loc='upper left', ncols=2)

if __name__ == "__main__":
    plt.show()
