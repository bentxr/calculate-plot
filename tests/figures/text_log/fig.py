# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(5.0, 3.5))
ax.plot([1, 10, 100], [1, 100, 10000])
ax.set_xscale('log')
ax.set_yscale('log')
ax.text(3.0, 300.0, '$y=x^2$ on log axes', rotation=35)
ax.text(0.0, 10.0, 'at x = 0: nowhere')
ax.text(10.0, -5.0, 'at y < 0: nowhere')
ax.text(200.0, 2.0, 'outside, to the right')

if __name__ == "__main__":
    plt.show()
