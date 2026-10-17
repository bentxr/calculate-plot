# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(8.0, 3.0))
axs[0].set_xscale('function', functions=(lambda x: x * 3, lambda x: x / 3))
axs[0].plot([1, 2, 3], [3, 1, 2])
t0 = axs[0].twinx()
t0.plot([1, 3], [10, 20], color='C3')
axs[1].set_xscale('log', base=2)
axs[1].plot([1, 8, 64], [1, 2, 3])
t1 = axs[1].twinx()
t1.plot([2, 32], [5, 1], color='C3')

if __name__ == "__main__":
    plt.show()
