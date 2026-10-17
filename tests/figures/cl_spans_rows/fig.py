# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(layout='constrained', figsize=(6.0, 6.0))
a = fig.add_subplot(3, 2, (1, 3))
b = fig.add_subplot(3, 2, 2)
c = fig.add_subplot(3, 2, (4, 6))
d = fig.add_subplot(3, 2, 5)
a.plot([0, 1], [0, 1])
a.set_title('a title over\ntwo rows of cells')
b.plot([0, 1], [5, 1])
b.set_xlabel("b's x\nlabel")
c.plot([0, 1], [1, 3])
c.set_xlabel('c spans the two lower rows')
c.set_title('c')
d.plot([0, 1], [2, 1])
d.set_title('d,\nwith a tall\ntitle')
fig.suptitle('rows that span')

if __name__ == "__main__":
    plt.show()
