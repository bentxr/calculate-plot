# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0.3,
    'savefig.dpi': 150,
})
fig, axs = plt.subplots(1, 2, figsize=(6.0, 2.5))
axs[0].plot([1, 10, 100], [1, 2, 3], label='data')
axs[0].set_xscale('log')
axs[0].legend()
axs[1].scatter([1, 2, 3], [10000, 10001, 10003])
axs[1].set_xlabel('x')

if __name__ == "__main__":
    plt.show()
