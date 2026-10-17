# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(layout='constrained', figsize=(7.0, 6.0))
top = fig.add_subplot(3, 3, (1, 3))
a = fig.add_subplot(3, 3, 4)
b = fig.add_subplot(3, 3, 5)
c = fig.add_subplot(3, 3, 6)
bottom = fig.add_subplot(3, 3, (7, 9))
top.plot([0, 1], [0, 1])
top.set_title('three columns')
a.plot([0, 1], [0, 1])
a.set_ylabel('a, on the right,\nin two lines')
a.yaxis.set_label_position('right')
b.plot([0, 1], [0, 100000])
b.set_ylabel("b's label\nof three\nlines")
c.plot([0, 1], [0, 1])
c.set_ylabel('c')
bottom.plot([0, 1], [1, 0])
bottom.set_xlabel('also three columns')

if __name__ == "__main__":
    plt.show()
