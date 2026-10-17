# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(layout='compressed')
a = fig.add_subplot(2, 2, 1)
b = fig.add_subplot(1, 2, 2)
a.plot([0, 1], [0, 1])
a.set_aspect('equal')
b.plot([0, 2], [0, 1])
b.set_aspect('equal')
b.set_title('another gridspec')

if __name__ == "__main__":
    plt.show()
