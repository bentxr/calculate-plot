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
    'xtick.minor.ndivs': 0,
    'ytick.minor.ndivs': 10,
    'savefig.bbox': 'tight',
})
fig, axs = plt.subplots(2, 2, figsize=(6.0, 4.0), sharex=True, sharey='row')
axs[0, 0].plot([0, 1, 2, 3], [1, 3, 2, 5])
axs[0, 1].plot([0, 10, 20, 30], [10, 30, 20, 50])
axs[1, 0].plot([5, 6], [0.1, 0.2])
axs[1, 1].plot([5, 6], [0.15, 0.4])

if __name__ == "__main__":
    plt.show()
