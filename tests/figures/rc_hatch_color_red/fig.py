# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'hatch.color': 'red',
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.scatter([1, 2, 3], [1, 3, 2], alpha=0.5)
ax.scatter([1, 2, 3], [2, 4, 3], c=[0.1, 0.5, 0.9], edgecolors='none')
ax.fill_between([1, 2, 3], [0, 1, 0], [1, 2, 1], alpha=0.3)
ax.vlines([1.5, 2.5], 0, 4)
ax.scatter([2], [0.5], marker='x')

if __name__ == "__main__":
    plt.show()
