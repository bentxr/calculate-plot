# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(6.0, 3.0), layout='compressed')
a = axs[0].imshow([[0.0, 0.0, -0.0, -0.0, -0.0, 0.0], [3.3659, 1.8186, -1.4007, -3.3322, -2.2001, 0.9548], [3.6372, 1.9652, -1.5136, -3.6008, -2.3774, 1.0317], [0.5645, 0.305, -0.2349, -0.5588, -0.369, 0.1601], [-3.0272, -1.6356, 1.2598, 2.9969, 1.9787, -0.8587]])
fig.colorbar(a, ax=axs[0])
b = axs[1].imshow([[0, 0, 0, 0], [0, 1, 2, 3], [0, 2, 4, 6], [0, 3, 6, 9]], cmap='cividis')
fig.colorbar(b, ax=axs[1], location='bottom', shrink=0.8)

if __name__ == "__main__":
    plt.show()
