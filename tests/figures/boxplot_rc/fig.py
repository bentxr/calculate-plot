# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'boxplot.vertical': False,
    'boxplot.medianprops.color': 'r',
    'boxplot.flierprops.marker': 'x',
    'boxplot.showmeans': True,
    'boxplot.patchartist': True,
    'boxplot.whiskers': [10, 90],
    'boxplot.meanprops.markersize': 9,
    'boxplot.boxprops.linewidth': 2.5,
    'boxplot.capprops.linestyle': '--',
})
fig, ax = plt.subplots()
ax.boxplot([[1, 2, 3, 4, 5, 6, 7, 8, 20], [2.5, 3, 3.2, 4, 5, 6.1, 9, -4]], label='box')
ax.legend()

if __name__ == "__main__":
    plt.show()
