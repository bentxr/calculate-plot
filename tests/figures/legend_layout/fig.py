# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'legend.handleheight': 1.4,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1], [0, 1], label='one')
ax.plot([0, 1], [0, 2], label='two\nlines')
ax.plot([0, 1], [0, 3], label='three', marker='o')
ax.plot([0, 1], [0, 4], label='four')
ax.plot([0, 1], [0, 5], label='five')
ax.legend(ncols=2, loc='upper left')

if __name__ == "__main__":
    plt.show()
