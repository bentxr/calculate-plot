# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.vlines([1.5, 2.5], 0, 3, linestyle='--', linestyles=':', color='blue', colors='red', label='v')
ax.hlines([1.0, 2.0], 1, 3, 'green', 'dashdot', lw=3.0, linewidths=1.0, label='h')
ax.legend()

if __name__ == "__main__":
    plt.show()
