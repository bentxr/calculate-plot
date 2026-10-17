# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(layout='constrained', figsize=(7.0, 5.5))
top = fig.add_subplot(3, 3, (1, 2))
corner = fig.add_subplot(3, 3, 3)
tall = fig.add_subplot(3, 3, (4, 7))
mid = fig.add_subplot(3, 3, 5)
right = fig.add_subplot(3, 3, (6, 9))
low = fig.add_subplot(3, 3, 8)
top.plot([0, 1, 2], [10000, 20000, 15000])
top.set_ylabel('a long label\non two lines')
top.set_title('spans two columns')
corner.plot([0, 1], [0, 1])
corner.set_xlabel('x of the corner')
tall.plot([0, 1, 2], [-1000, 0, 1000])
tall.set_ylabel('spans two rows, with a long label')
tall.set_xlabel('x\nof the tall one')
mid.plot([0, 1], [1, 0])
mid.set_title('a title\non two lines')
right.plot([0, 1, 2], [3, 1, 2])
right.set_ylabel('right')
right.yaxis.set_label_position('right')
low.plot([0, 100000], [0, 1])
low.set_xlabel('the lowest x label,\nthree lines\nlong')

if __name__ == "__main__":
    plt.show()
