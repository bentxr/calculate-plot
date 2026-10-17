# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(7.0, 3.0))
axs[0].plot([0, 1, 2], [0, 1, 4])
axs[0].text(2.0, 2.0, 'sticks out to the right', fontsize=12)
axs[1].plot([0, 1, 2], [4, 1, 0])
axs[1].text(1.0, 4.4, 'above the axes\nin two lines', ha='center')
axs[1].text(1.0, -1.0, 'clipped below', clip_on=True)
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
