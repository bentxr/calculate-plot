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
})
fig, axs = plt.subplots(1, 2)
axs[0].annotate('far out', (0.5, 0.5), (1.3, 1.2), xycoords='axes fraction', arrowprops={'arrowstyle': '->', 'connectionstyle': 'arc3,rad=0.5'})
axs[1].annotate('below', (0.5, 0.5), (0.2, -0.3), xycoords='axes fraction', arrowprops={'shrink': 0.1})
plt.tight_layout()

if __name__ == "__main__":
    plt.show()
