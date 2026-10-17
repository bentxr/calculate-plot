# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'grid.color': '#00000060',
    'grid.alpha': 0.9,
})
fig, axs = plt.subplots(2, 2, figsize=(5.0, 4.0))
axs[0, 0].grid(color='blue')
axs[0, 1].grid(color='#ff000080')
axs[0, 1].grid(color='green', ls='--')
axs[0, 1].grid(linestyle=':', lw=2.0)
axs[1, 0].grid(alpha=0.3)
axs[1, 0].grid(c='#ff000080')
axs[1, 1].grid(alpha=0.3, which='both')
axs[1, 1].grid(color='#0000ff40', which='minor')
axs[1, 1].set_yscale('log')

if __name__ == "__main__":
    plt.show()
