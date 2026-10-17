# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(layout='constrained', figsize=(6.0, 4.5))
a = fig.add_subplot(2, 2, 1)
b = fig.add_subplot(2, 2, 3)
c = fig.add_subplot(1, 2, 2)
a.plot([1, 2], [100000, 200000])
b.set_xlabel('bottom')
c.set_ylabel('right column')
ct = c.twinx()
ct.set_ylabel('twin')
c.inset_axes([0.5, 0.5, 0.4, 0.4])

if __name__ == "__main__":
    plt.show()
