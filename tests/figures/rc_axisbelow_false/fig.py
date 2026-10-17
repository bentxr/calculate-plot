# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.axisbelow': 'False',
    'xtick.direction': 'in',
    'ytick.direction': 'inout',
    'xtick.major.size': 12,
    'ytick.major.size': 12,
    'xtick.major.width': 3,
    'ytick.major.width': 3,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2, 3], [0, 3, 1, 2], linewidth=8.0)
ax.axhspan(0.5, 1.5, color='orange')
ax.fill_between([0, 1, 2, 3], [2, 2.5, 2, 2.5], [3, 3, 3, 3], color='0.3')
ax.scatter([0, 1, 2, 3], [1, 1, 2, 2], s=900)
ax.vlines([1.0, 2.0], 0, 3, linewidth=6.0, colors='k')
ax.text(1.0, 2.0, 'over the grid', fontsize=16)
ax.grid(True, color='red', linewidth=3.0)
ax.legend(loc='center')

if __name__ == "__main__":
    plt.show()
