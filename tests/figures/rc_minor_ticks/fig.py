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
    'axes.grid': True,
    'axes.grid.which': 'both',
    'grid.alpha': 0.4,
})
fig, axs = plt.subplots(2, 2, figsize=(8.0, 6.0))
axs[0, 0].plot([0, 1, 2, 3], [1, 3, 2, 5])
axs[0, 1].plot([0, 250, 500, 1000], [-0.02, 0.01, 0.03, 0.04])
axs[1, 0].plot([1, 2, 3, 4], [1, 30, 200, 5000])
axs[1, 0].set_yscale('log')
axs[1, 1].plot([1234500.0, 1234507.0], [3, 7])
axs[1, 1].set_xlim(1234509.0, 1234499.0)
axs[1, 1].set_ylim(0, 7.5)
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
