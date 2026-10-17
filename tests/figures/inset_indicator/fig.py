# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(9, 4))
axs[0].plot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [0, 1, 0, 3, 2, 5, 3, 6, 4, 8])
ins = axs[0].inset_axes([0.55, 0.08, 0.4, 0.35])
ins.plot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [0, 1, 0, 3, 2, 5, 3, 6, 4, 8])
ins.set(xlim=(2, 4), ylim=(0, 3.5))
axs[0].indicate_inset_zoom(ins, edgecolor='black')
axs[1].plot([0, 10], [0, 10])
ins2 = axs[1].inset_axes([0.05, 0.6, 0.35, 0.35])
ins2.set(xlim=(6, 8), ylim=(1, 3))
axs[1].indicate_inset_zoom(ins2, lw=2, ls='--', alpha=0.8)
axs[1].indicate_inset([1, 7, 2, 2], facecolor='C1', alpha=0.3)

if __name__ == "__main__":
    plt.show()
