# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.axhspan(1.0, 2.0, ec='blue', edgecolor='red', fc='yellow', facecolor='green', lw=3.0, linewidth=1.0, ls='--', linestyle=':')
ax.axvspan(0.2, 0.4, facecolor='blue', color='red', alpha=0.3)
ax.axvspan(0.6, 0.8, color='red', edgecolor='blue', facecolor='yellow', linewidth=4.0)
ax.axhspan(3.0, 4.0, fc='C2', color='C3', ec='black')

if __name__ == "__main__":
    plt.show()
