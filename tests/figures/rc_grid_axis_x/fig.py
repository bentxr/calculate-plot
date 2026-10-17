# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.grid': True,
    'axes.grid.which': 'both',
    'axes.grid.axis': 'x',
})
fig, axs = plt.subplots(1, 2, figsize=(7.0, 3.0))
axs[0].plot([0, 1, 2, 3], [1, 3, 2, 5])
axs[1].plot([1, 2, 3, 4], [1, 30, 200, 5000])
axs[1].set_yscale('log')
axs[1].set_xscale('log')

if __name__ == "__main__":
    plt.show()
