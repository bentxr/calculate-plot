# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(layout='constrained', figsize=(7.0, 6.0))
left = fig.add_subplot(3, 3, (1, 7))
a = fig.add_subplot(3, 3, 2)
b = fig.add_subplot(3, 3, 5)
c = fig.add_subplot(3, 3, 8)
right = fig.add_subplot(3, 3, (3, 9))
left.plot([0, 1], [0, 1])
left.set_ylabel('three rows')
a.plot([0, 1], [0, 1])
a.set_title('a')
a.set_xlabel("a's x label\nin two lines")
b.plot([0, 1], [0, 1])
b.set_title("b's title\nof three\nlines")
b.set_xlabel('b')
c.plot([0, 1], [0, 1])
c.set_title('c, two\nlines')
right.plot([0, 1], [1, 0])
right.set_title('also three rows')

if __name__ == "__main__":
    plt.show()
