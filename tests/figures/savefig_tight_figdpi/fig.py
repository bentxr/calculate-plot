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
    'savefig.pad_inches': 0.05,
})
fig, axs = plt.subplots(2, 1, figsize=(3.0, 3.0), dpi=80, sharex=True)
axs[0].plot([0, 1, 2], [3, 1, 2])
axs[1].plot([0, 1, 2], [0.001, 0.002, 0.004])
axs[1].set_xlabel('shared')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
