# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, sharex=True, figsize=(9.0, 3.0))
axs[0].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[1].plot([0.6, 3.4, 4.4, 9.8, 12.2], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[0].axis([0, 3, 0, 1], emit=False)
axs[2].plot([1.1, -0.4, 2.8, 0.9, 3.7], [0.3, 1.7, 2.2, 4.9, 6.1])
axs[2].axis('auto')
axs[2].set_xlim(-2, 5, auto=None, emit=False)
plt.axis('EQUAL')

if __name__ == "__main__":
    plt.show()
