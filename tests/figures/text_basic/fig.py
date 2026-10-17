# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(6.0, 4.5))
ax.plot([0, 1, 2], [0, 1, 4])
ax.text(0.1, 3.5, 'plain text')
ax.text(1.0, 1.0, 'at the point', color='C3', fontsize=14)
ax.text(0.5, 2.0, 'turned', rotation=30, ha='center', va='center')
ax.text(1.5, 0.5, 'two\nlines here', horizontalalignment='right', verticalalignment='top', size='large')
ax.text(1.9, 3.9, 'sticks out of the axes', va='bottom')
ax.text(x=0.2, y=0.2, s='by keywords')
ax.text(1.2, 2.6, '$\\frac{\\partial f}{\\partial x}=\\sqrt{x}$', rotation=45, fontsize=16)

if __name__ == "__main__":
    plt.show()
