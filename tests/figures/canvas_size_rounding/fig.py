# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(5.1, 2.3))
ax.plot([0, 1, 2], [0, 1, 4])
ax.set_title('510 by 230 pixels')
ax.set_ylabel('y')

if __name__ == "__main__":
    plt.show()
