# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(7.5, 2.5))
axs[0].tripcolor([0, 1, 0.5, 1.5, 2], [0, 0, 1, 1, 0], [0.1, 0.9, 0.4, 0.7, 0.2], shading='gouraud', cmap='viridis')
axs[0].axis('off')
axs[1].pcolormesh([0, 1, 2], [0, 1, 2], [[0, 1, 2], [1, 2, 3], [2, 3, 4]], shading='gouraud', cmap='plasma', alpha=0.6)
axs[1].axis('off')
axs[2].pcolormesh([[0, 1, 2], [3, 4, 5]], cmap='gray', edgecolors='C1', linewidth=2)
axs[2].pcolormesh([3, 4, 5], [0, 1, 2], [[0, 1, 2], [1, 2, 3], [2, 3, 4]], shading='gouraud', cmap='coolwarm')
axs[2].axis('off')

if __name__ == "__main__":
    plt.show()
