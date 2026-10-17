# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'xtick.minor.visible': True,
    'ytick.minor.visible': True,
    'xtick.major.bottom': False,
    'ytick.major.left': False,
})
fig, axs = plt.subplots(2, 5, figsize=(7.0, 2.5))
axs[0, 0].plot([0, 1, 2, 3], [1, 3, 2, 5])
axs[0, 1].plot([0, 250, 500, 1000], [-0.02, 0.01, 0.03, 0.04])
axs[1, 2].plot([1, 2, 3, 4], [1, 30, 200, 5000])
axs[1, 3].plot([12.5, 13.5], [3, 7])
axs[1, 4].set_yscale('log')

if __name__ == "__main__":
    plt.show()
