# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(5.0, 2.5))
axs[0].imshow([[0.0, 0.0, 0.0, 0.0, -0.0], [1.932653, 1.696063, 1.044217, 0.13671, -0.804267], [2.956349, 2.59444, 1.597322, 0.209124, -1.230275], [2.589628, 2.272612, 1.399182, 0.183183, -1.077666]], interpolation='none')
axs[1].imshow([[[0.0, 0.0, 0.0], [0.0, 0.5, 0.25], [0.0, 1.0, 0.5]], [[0.5, 0.0, 0.25], [0.5, 0.5, 0.5], [0.5, 1.0, 0.75]], [[1.0, 0.0, 0.5], [1.0, 0.5, 0.75], [1.0, 1.0, 1.0]]], interpolation='none')
axs[1].set_xlim(-1, 2)

if __name__ == "__main__":
    plt.show()
