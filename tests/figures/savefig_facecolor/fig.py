# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'savefig.facecolor': '#ddeeff',
    'savefig.edgecolor': 'red',
    'figure.facecolor': 'yellow',
})
fig, ax = plt.subplots(figsize=(3.0, 2.0))
ax.plot([0, 1], [0, 1])

if __name__ == "__main__":
    plt.show()
