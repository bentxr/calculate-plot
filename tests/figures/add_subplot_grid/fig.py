# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(figsize=(6.4, 4.8))
ax1 = fig.add_subplot(2, 2, 1)
ax2 = fig.add_subplot(222)
ax3 = fig.add_subplot(2, 2, (3, 4))
ax1.plot([0, 1, 2], [1, 0, 1])
ax2.scatter([0, 1, 2], [1, 2, 0])
ax3.plot([0, 1, 2, 3], [3, 1, 2, 0], label='wide')
ax3.set_title('spans two cells')
ax3.legend()
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
