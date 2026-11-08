# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(figsize=(7.0, 4.5), layout='constrained')
outer = fig.add_gridspec(1, 2, width_ratios=[1, 1.5])
left = outer[0].subgridspec(3, 1, hspace=0)
a = fig.add_subplot(left[0:2])
a.plot([0, 1, 2], [3, 1, 2])
a.set_ylabel('two rows')
b = fig.add_subplot(left[2])
b.set_xlabel('the last row')
right = outer[1].subgridspec(2, 2, width_ratios=[1, 3], wspace=0.1)
deep = right[0, :].subgridspec(1, 3)
d0 = fig.add_subplot(deep[0, 0])
d0.set_title('deep')
fig.add_subplot(deep[0, 1:])
fig.add_subplot(right[1, 0])
e = fig.add_subplot(right[1, 1])
e.set_ylabel('e')
fig.suptitle('Grids inside grids')

if __name__ == "__main__":
    plt.show()
