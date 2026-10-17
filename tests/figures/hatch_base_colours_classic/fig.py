# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.style.use('classic')
fig, ax = plt.subplots()
ax.axhspan(0, 0.8, color='k', hatch='-', alpha=None, label='k')
ax.axhspan(1, 1.8, color='b', hatch='/', alpha=None, label='b')
ax.axhspan(2, 2.8, color='r', hatch='-', alpha=0.6, label='r')
ax.axhspan(3, 3.8, color='w', hatch='/', alpha=None, label='w')
ax.axhspan(4, 4.8, color='C0', hatch='-', alpha=None, label='C0')
ax.axhspan(5, 5.8, color='C3', hatch='/', alpha=None, label='C3')
ax.axhspan(6, 6.8, color='red', hatch='-', alpha=None, label='red')
ax.axhspan(7, 7.8, color='#336699', hatch='/', alpha=None, label='#336699')
ax.axvspan(0.2, 0.4, facecolor='y', hatch='x', alpha=1)
ax.legend(fontsize=6)

if __name__ == "__main__":
    plt.show()
