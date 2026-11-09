# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(figsize=(7.0, 4.0))
sfigs = fig.subfigures(1, 2, width_ratios=[2, 1], wspace=0.07)
sfigs[0].set_facecolor('0.9')
axs = sfigs[0].subplots(2, 1, sharex=True)
axs[0].plot([0, 1, 2, 3], [1, 3, 2, 4], label='rising')
axs[1].plot([0, 1, 2, 3], [4, 2, 3, 1], label='falling', color='C1')
axs[0].set_aspect(0.5)
sfigs[0].suptitle('Left')
p = sfigs[1].add_subplot(111, projection='polar')
p.plot([0, 1, 2], [1, 2, 1], label='polar')
sfigs[1].suptitle('Right', fontsize='small')
sfigs[1].text(0.05, 0.05, 'text in a subfigure')
sfigs[1].legend(loc='lower right')
fig.suptitle('Subfigures', y=1.0, va='top')
fig.legend(loc='upper left', fontsize=7)

if __name__ == "__main__":
    plt.show()
