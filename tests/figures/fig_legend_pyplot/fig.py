# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'legend.loc': 'upper left',
    'savefig.bbox': 'tight',
})
plt.subplot(2, 1, 1)
plt.plot([0, 1, 2], [2, 0, 1], label='top')
plt.twinx()
plt.plot([0, 1, 2], [10, 30, 20], label='top, right axis', color='C1')
plt.subplot(2, 1, 2)
plt.step([0, 1, 2], [1, 2, 0], label='bottom')
plt.legend()
plt.figlegend(loc='center left', shadow=True, bbox_transform=None)
plt.figlegend(loc='upper right', borderaxespad=0)
plt.figlegend(loc=True)

if __name__ == "__main__":
    plt.show()
