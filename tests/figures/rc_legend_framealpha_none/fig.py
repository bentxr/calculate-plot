# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'legend.framealpha': None,
    'legend.facecolor': '#ffff0080',
    'legend.edgecolor': '#0000ff40',
})
fig, axs = plt.subplots(1, 2, figsize=(6.0, 2.5))
axs[0].plot([0, 1, 2], [0, 2, 1], label='a line', linewidth=6.0)
axs[0].legend(loc='center')
axs[1].plot([0, 1, 2], [0, 2, 1], label='a line', linewidth=6.0)
axs[1].legend(loc='center', framealpha=0.3)

if __name__ == "__main__":
    plt.show()
