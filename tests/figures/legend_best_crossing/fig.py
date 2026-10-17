# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([-10, 20], [-10, 20], label='rising')
ax.plot([-10, 20], [20, -10], label='falling')
ax.plot([-10, 20], [5, 5], label='middle')
ax.set_xlim(0, 10)
ax.set_ylim(0, 10)
ax.legend()

if __name__ == "__main__":
    plt.show()
