# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.imshow([[0.0, 0.0, 0.0, 0.0, -0.0], [1.932653, 1.696063, 1.044217, 0.13671, -0.804267], [2.956349, 2.59444, 1.597322, 0.209124, -1.230275], [2.589628, 2.272612, 1.399182, 0.183183, -1.077666]], extent=[0, 5, 0, 4])
ax.imshow([[0, 1, 2], [3, 4, 5], [6, 7, 8]], extent=[6, 9, 1, 4], cmap='hot', alpha=0.8)
ax.set_xlim(-1, 10)

if __name__ == "__main__":
    plt.show()
