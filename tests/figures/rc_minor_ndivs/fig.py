# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'xtick.minor.visible': 'yes',
    'ytick.minor.visible': True,
    'xtick.minor.ndivs': 2,
    'ytick.minor.ndivs': '3',
    'xtick.minor.size': 4,
    'ytick.minor.width': 1.5,
    'xtick.top': True,
    'ytick.right': True,
    'xtick.direction': 'in',
    'xtick.minor.top': False,
})
fig, axs = plt.subplots(1, 3, figsize=(9.0, 3.0))
axs[0].plot([0, 1, 2, 3], [1, 3, 2, 5])
axs[1].plot([0.001, 0.002, 0.003], [10, 30, 20])
axs[1].grid(True, which='minor', axis='y')
axs[2].plot([1, 2, 3], [0, 5, 2])
axs[2].set_xscale('log')
fig.tight_layout()
axs[2].set_xscale('linear')

if __name__ == "__main__":
    plt.show()
