# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.2, 5.4))
bars = axs[0, 0].bar([1, 2, 3, 4], [3, -2, 5, 1.5], yerr=[0.5, 0.3, 0.2, 0.1])
axs[0, 0].bar_label(bars)
axs[0, 0].bar_label(bars, label_type='center', fmt='%.1f', color='w')
hb = axs[0, 1].barh([1, 2, 3], [4, -1, 2.5])
axs[0, 1].set_xlim(6, -3)
axs[0, 1].bar_label(hb, ['a', 'b', 'c'], padding=3)
ib = axs[1, 0].bar([0, 1, 2, 3], [7, 3, 0, 12])
axs[1, 0].set_ylim(0, 10)
axs[1, 0].bar_label(ib, fmt='{:.2f}', padding=[1, 2, 3, 4])
axs[1, 0].bar_label(ib, fmt='%d%%', label_type='center', fontsize=8)
nb = axs[1, 1].bar([1, 2, 3], [float('nan'), 2, -1], bottom=0.5)
axs[1, 1].set_ylim(3, -2)
axs[1, 1].bar_label(nb, fmt='%s')

if __name__ == "__main__":
    plt.show()
