# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axd = plt.subplot_mosaic([['A', [['B', [['C'], ['D']]], ['E', 'E']]]], layout='constrained', width_ratios=[2, 3], figsize=(7.0, 4.5))
axd['A'].set_ylabel('a long label on the left')
axd['B'].set_title('B')
axd['C'].plot([0, 1000], [0, 1])
axd['D'].set_xlabel("D's label\non two lines")
axd['E'].set_ylabel('E')
fig.suptitle('Nested grids')

if __name__ == "__main__":
    plt.show()
