# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(6.4, 3.0))
axs[0].hlines([float('nan'), 2, 3.5], 5.2, 1000.0, label='y bad')
axs[0].hlines([1, 2.5], [5.2, float('nan')], 800.0, colors='C1')
axs[0].hlines([float('nan')], 300, 900, colors='C2', label='one, bad')
axs[0].legend()
axs[1].vlines([1, 2, 3], 0, [5, float('inf'), 2])
axs[1].vlines([float('-inf'), 4], [1, 2], [3, 4], colors='C3', linestyles='dashed')
axs[1].vlines(5, float('nan'), 2)

if __name__ == "__main__":
    plt.show()
