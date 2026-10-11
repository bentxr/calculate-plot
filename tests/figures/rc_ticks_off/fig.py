# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'xtick.bottom': False,
    'ytick.major.left': False,
})
fig, axs = plt.subplots(2, 2, figsize=(6.0, 4.0), sharex='col')
axs[0, 0].plot([0.5, 1, 2, 4], [1, 3, 2, 5])
axs[0, 1].plot([0.5, 1, 2, 4], [10, 30, 20, 50])
axs[1, 0].plot([1, 2, 3], [0.1, 0.3, 0.2])
axs[1, 1].plot([1, 2, 3], [1, 100, 10])
axs[1, 1].set_yscale('log')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
