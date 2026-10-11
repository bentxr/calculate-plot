# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.titlesize': 15,
    'axes.labelsize': 8.5,
    'xtick.labelsize': 7,
    'ytick.labelsize': 12,
    'legend.fontsize': 6.5,
    'figure.titlesize': 18,
    'axes.titleweight': 700,
    'axes.labelweight': 600,
    'figure.titleweight': 300,
    'font.weight': 500,
    'font.size': 11,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2], [1000000, 2000000, 4000000], label='numbers for sizes')
ax.set_title('Title at 15 pt, weight 700')
ax.set_xlabel('x label at 8.5 pt')
ax.set_ylabel('y label')
ax.legend()
fig.suptitle('Suptitle at 18 pt')

if __name__ == "__main__":
    plt.show()
