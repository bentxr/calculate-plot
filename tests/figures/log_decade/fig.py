# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(5.0, 3.0))
ax.plot([2, 3, 5, 7.5], [0.31, 0.29, 0.6, 0.45], marker='o')
ax.set_xscale('log')
ax.set_yscale('log')

if __name__ == "__main__":
    plt.show()
