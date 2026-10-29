# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 4, figsize=(13.0, 6.0))
axs[0, 0].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[0, 0].axis('off')
axs[0, 1].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[0, 1].axis('equal')
axs[0, 2].scatter([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[0, 2].axis('scaled')
axs[0, 3].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[0, 3].axis('Tight')
axs[0, 3].plot([8, 9], [5, 6])
axs[1, 0].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[1, 0].axis('image')
axs[1, 1].plot([0.3, 1.7, 2.2, 4.9, 6.1], [3.3000000000000003, -1.2000000000000002, 8.399999999999999, 2.7, 11.100000000000001])
axs[1, 1].axis('square')
axs[1, 2].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[1, 2].axis([0, 5, -1, 4])
axs[1, 3].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[1, 3].axis(xmin=1, ymax=3)
axs[1, 3].plot([0, 8], [-3, 6])
plt.axis(True)

if __name__ == "__main__":
    plt.show()
