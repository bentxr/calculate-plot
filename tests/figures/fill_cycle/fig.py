# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.prop_cycle': "cycler(color=['r', 'g', 'b']) + cycler(linestyle=['-', '--', ':'])",
})
fig, ax = plt.subplots()
ax.fill([0, 1, 2], [0, 1, 0], edgecolor='k')
ax.fill([0, 1, 2], [1, 2, 1], edgecolor='k')
ax.bar([3, 4], [1, 2], edgecolor='k')
ax.fill([5, 6, 7], [0, 1, 0], color='C5', edgecolor='k')
ax.fill([5, 6, 7], [1, 2, 1], linestyle=':', edgecolor='k')
ax.fill([8, 9, 10], [0, 1, 0], 'm', edgecolor='k', linewidth=2)
ax.fill_between([0, 10], [3.6, 3.9], 3.4)
ax.plot([0, 10], [2.5, 2.5])

if __name__ == "__main__":
    plt.show()
