# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.prop_cycle': "cycler(lw=[1, 3]) * cycler(ls=['-', '--'])",
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1], [0, 1], [0, 1], [1, 2], [0, 1], [2, 3], [0, 1], [3, 4], [0, 1], [4, 5])
ax.plot([0, 1], [5, 6], color='C2', lw=2, ls=':')
ax.plot([0, 1], [6, 7], color='C2')
ax.scatter([0.2, 0.8], [1, 5])
ax.scatter([0.3, 0.7], [1, 5])
ax.fill_between([0, 1], [6.5, 6.5], [7, 7.5], alpha=0.5)
ax.errorbar([0.5], [3.5], yerr=0.5, capsize=5)

if __name__ == "__main__":
    plt.show()
