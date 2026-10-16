# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'markers.fillstyle': 'bottom',
})
fig, ax = plt.subplots()
ax.plot([0, 1, 2], [0, 1, 0], 's-', markerfacecoloralt='C1', markersize=12)
ax.scatter([0, 1, 2], [1, 2, 1], marker='o', s=200)
ax.plot([0, 1, 2], [2, 3, 2], 'h', fillstyle='full')

if __name__ == "__main__":
    plt.show()
