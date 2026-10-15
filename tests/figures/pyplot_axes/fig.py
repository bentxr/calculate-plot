# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.axes((0.1, 0.1, 0.5, 0.5))
plt.plot([0, 1, 2], [1, 0, 1])
plt.axes(position=[0.6, 0.6, 0.3, 0.3])
plt.plot([0, 1, 2], [0, 1, 0], color='C3')
plt.title('inset')
ax = plt.gca()
ax.set_axis_off()

if __name__ == "__main__":
    plt.show()
