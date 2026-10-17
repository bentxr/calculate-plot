# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([1, 2, 3], [5, 5, 5], '')
ax.plot([2], [5], '1', ms=3)
ax.plot([1.5, 2.5], [5.01, 4.99], '^', [1, 3], [4.995, 5.005], 'k-', markersize=3)
ax.plot([], [])

if __name__ == "__main__":
    plt.show()
