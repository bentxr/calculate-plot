# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axd = plt.subplot_mosaic([[1, [['a', 'b'], ['c', 'b']], '.'], [2, 2, 'p']], figsize=(9.0, 6.0), gridspec_kw={'wspace': 0.4, 'hspace': 0.3, 'left': 0.08}, sharey=True, per_subplot_kw={'p': {'projection': 'polar'}})
axd[1].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axd[2].bar([1, 2, 3], [3, 1, 2])
axd['a'].scatter([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axd["b"].plot([1.1, -0.4, 2.8, 0.9, 3.7], [0.3, 1.7, 2.2, 4.9, 6.1])
axd['c'].plot([0.3, 1.7, 2.2, 4.9, 6.1], [0.3, 1.7, 2.2, 4.9, 6.1])
axd['p'].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, 0.4, 2.8, 0.9, 3.7])
plt.ylabel('current')

if __name__ == "__main__":
    plt.show()
