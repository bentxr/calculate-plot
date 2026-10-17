# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.scatter([0, 1, 2, 3, 4], [2, 1, 4, 3, 5])
ax.scatter([0.5, 1.5, 2.5], [4, 2, 1], s=[20, 80, 200], c='C3', marker='s', alpha=0.7)

if __name__ == "__main__":
    plt.show()
