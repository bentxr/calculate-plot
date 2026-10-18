# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt
from matplotlib import colors

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

bn = colors.BoundaryNorm([-4, -2, -1, 0, 1, 2, 4], ncolors=256)
bn2 = colors.BoundaryNorm([-4, -2, -1, 0, 1, 2, 4], ncolors=256, extend='both')
pn = colors.PowerNorm(0.5)
tn = colors.TwoSlopeNorm(vcenter=1.0)
cn = colors.CenteredNorm()
fig, axs = plt.subplots(2, 3, figsize=(8.0, 5.0))
a = axs[0, 0].imshow([[0.0, 0.0, -0.0, -0.0, -0.0, 0.0], [3.3659, 1.8186, -1.4007, -3.3322, -2.2001, 0.9548], [3.6372, 1.9652, -1.5136, -3.6008, -2.3774, 1.0317], [0.5645, 0.305, -0.2349, -0.5588, -0.369, 0.1601], [-3.0272, -1.6356, 1.2598, 2.9969, 1.9787, -0.8587]], norm=bn)
fig.colorbar(a, ax=axs[0, 0])
b = axs[0, 1].imshow([[0.0, 0.0, -0.0, -0.0, -0.0, 0.0], [3.3659, 1.8186, -1.4007, -3.3322, -2.2001, 0.9548], [3.6372, 1.9652, -1.5136, -3.6008, -2.3774, 1.0317], [0.5645, 0.305, -0.2349, -0.5588, -0.369, 0.1601], [-3.0272, -1.6356, 1.2598, 2.9969, 1.9787, -0.8587]], norm=bn2, cmap='RdBu')
fig.colorbar(b, ax=axs[0, 1], spacing='proportional')
c = axs[0, 2].imshow([[0, 1, 2, 3], [4, 5, 6, 7], [8, 9, 0, 1]], norm='linear', cmap='tab10', vmin=0, vmax=9)
fig.colorbar(c, ax=axs[0, 2])
d = axs[1, 0].imshow([[0, 0, 0, 0, 0], [0, 1, 2, 3, 4], [0, 2, 4, 6, 8], [0, 3, 6, 9, 12], [0, 4, 8, 12, 16]], norm=pn)
fig.colorbar(d, ax=axs[1, 0])
e = axs[1, 1].imshow([[0.0, 0.0, -0.0, -0.0, -0.0, 0.0], [3.3659, 1.8186, -1.4007, -3.3322, -2.2001, 0.9548], [3.6372, 1.9652, -1.5136, -3.6008, -2.3774, 1.0317], [0.5645, 0.305, -0.2349, -0.5588, -0.369, 0.1601], [-3.0272, -1.6356, 1.2598, 2.9969, 1.9787, -0.8587]], norm=tn, cmap='coolwarm')
fig.colorbar(e, ax=axs[1, 1], extend='min')
f = axs[1, 2].imshow([[0.0, 0.0, -0.0, -0.0, -0.0, 0.0], [3.3659, 1.8186, -1.4007, -3.3322, -2.2001, 0.9548], [3.6372, 1.9652, -1.5136, -3.6008, -2.3774, 1.0317], [0.5645, 0.305, -0.2349, -0.5588, -0.369, 0.1601], [-3.0272, -1.6356, 1.2598, 2.9969, 1.9787, -0.8587]], norm=cn, cmap='PRGn')
fig.colorbar(f, ax=axs[1, 2], orientation='horizontal')

if __name__ == "__main__":
    plt.show()
