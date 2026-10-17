# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.fill_between([0, 1, 2, 3, 4], [1, 3, 2, 4, 3], [0, 0.5, 1, 1.5, 1], alpha=0.4)

if __name__ == "__main__":
    plt.show()
