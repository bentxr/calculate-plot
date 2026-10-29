# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(9.0, 7.0))
axs[0, 0].stackplot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 2, 3, 2, 1, 2, 4, 3, 2, 1], [1.0, 1.421, 1.455, 1.071, 0.622, 0.521, 0.86, 1.328, 1.495, 1.206], [2, 2, 1, 1, 3, 3, 2, 1, 1, 2], labels=['a', 'b', 'c'])
axs[0, 0].legend(loc='upper left')
axs[0, 1].stackplot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 2, 3, 2, 1, 2, 4, 3, 2, 1], [1.0, 1.421, 1.455, 1.071, 0.622, 0.521, 0.86, 1.328, 1.495, 1.206], [2, 2, 1, 1, 3, 3, 2, 1, 1, 2], baseline='sym', colors=['r', 'g', 'b'], alpha=0.5)
axs[1, 0].stackplot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 2, 3, 2, 1, 2, 4, 3, 2, 1], [1.0, 1.421, 1.455, 1.071, 0.622, 0.521, 0.86, 1.328, 1.495, 1.206], [2, 2, 1, 1, 3, 3, 2, 1, 1, 2], baseline='wiggle', edgecolor='k', hatch=['/', '.', None], linewidth=[1, 2])
plt.stackplot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 2, 3, 2, 1, 2, 4, 3, 2, 1], [1.0, 1.421, 1.455, 1.071, 0.622, 0.521, 0.86, 1.328, 1.495, 1.206], [2, 2, 1, 1, 3, 3, 2, 1, 1, 2], baseline='weighted_wiggle')

if __name__ == "__main__":
    plt.show()
