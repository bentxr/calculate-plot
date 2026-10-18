# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt
from matplotlib import colors

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

lognorm = colors.LogNorm(vmin=0.01, vmax=100)
bounds = colors.BoundaryNorm([-3, -1, 0, 1, 3], ncolors=256, extend='both')
twoslope = colors.TwoSlopeNorm(vcenter=0.5)
fig, axs = plt.subplots(2, 3, figsize=(7.0, 4.5))
axs[0, 0].imshow([[1.0, 0.316228, 0.1, 0.031623, 0.01], [3.162278, 1.0, 0.316228, 0.1, 0.031623], [10.0, 3.162278, 1.0, 0.316228, 0.1], [31.622777, 10.0, 3.162278, 1.0, 0.316228], [100.0, 31.622777, 10.0, 3.162278, 1.0]], norm=lognorm)
axs[0, 1].imshow([[1.0, 2.154435, 4.641589, 10.0, 21.544347], [2.154435, 4.641589, 10.0, 21.544347, 46.415888], [4.641589, 10.0, 21.544347, 46.415888, 100.0], [10.0, 21.544347, 46.415888, 100.0, 215.443469], [21.544347, 46.415888, 100.0, 215.443469, 464.158883]], norm='log')
axs[0, 2].imshow([[0.0, 0.0, 0.0, 0.0, -0.0], [1.932653, 1.696063, 1.044217, 0.13671, -0.804267], [2.956349, 2.59444, 1.597322, 0.209124, -1.230275], [2.589628, 2.272612, 1.399182, 0.183183, -1.077666]], norm=bounds, cmap='RdBu')
axs[1, 0].imshow([[0.0, 0.0, 0.0, 0.0, -0.0], [1.932653, 1.696063, 1.044217, 0.13671, -0.804267], [2.956349, 2.59444, 1.597322, 0.209124, -1.230275], [2.589628, 2.272612, 1.399182, 0.183183, -1.077666]], norm=twoslope, cmap='PiYG')
axs[1, 1].imshow([[0, 1, 2, 3], [4, 5, 6, 7], [8, 9, 0, 1]], norm='symlog', cmap='tab10')
axs[1, 2].imshow([[0.0, 0.0, 0.0, 0.0, -0.0], [1.932653, 1.696063, 1.044217, 0.13671, -0.804267], [2.956349, 2.59444, 1.597322, 0.209124, -1.230275], [2.589628, 2.272612, 1.399182, 0.183183, -1.077666]], norm='linear', vmin=-1, vmax=1)

if __name__ == "__main__":
    plt.show()
