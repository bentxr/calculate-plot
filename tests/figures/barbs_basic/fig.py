# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(8.0, 6.0))
axs[0, 0].barbs([0, 1, 2, 3, 4], [0, 1, 2, 3], [[20.0, 16.51, 7.25, -4.54, -14.75], [19.11, 12.43, 1.41, -10.1, -18.08], [16.51, 7.25, -4.54, -14.75, -19.8], [12.43, 1.41, -10.1, -18.08, -19.75]], [[0.0, 9.74, 17.93, 23.3, 24.99], [-11.99, -2.5, 7.39, 16.11, 22.28], [-21.04, -14.12, -4.97, 4.97, 14.12], [-24.94, -22.28, -16.11, -7.39, 2.5]])
axs[0, 1].barbs([0, 1, 2, 3, 4, 5, 6], [0, 0, 0, 0, 0, 0, 0], [0, 3, 8, 15, 40, 65, 120], [0, 1, -2, 5, 10, -30, 0], length=9, pivot='middle', barbcolor='b', flagcolor='r', fill_empty=True, sizes={'emptybarb': 0.3, 'spacing': 0.2})
axs[0, 1].set_xlim(-1, 7)
axs[1, 0].barbs([[20.0, 16.51, 7.25, -4.54, -14.75], [19.11, 12.43, 1.41, -10.1, -18.08], [16.51, 7.25, -4.54, -14.75, -19.8], [12.43, 1.41, -10.1, -18.08, -19.75]], [[0.0, 9.74, 17.93, 23.3, 24.99], [-11.99, -2.5, 7.39, 16.11, 22.28], [-21.04, -14.12, -4.97, 4.97, 14.12], [-24.94, -22.28, -16.11, -7.39, 2.5]], [[0.0, 1.0, 2.0, 3.0, 4.0], [1.0, 2.0, 3.0, 4.0, 5.0], [2.0, 3.0, 4.0, 5.0, 6.0], [3.0, 4.0, 5.0, 6.0, 7.0]], cmap='viridis', rounding=False, barb_increments={'half': 2, 'full': 4, 'flag': 20}, flip_barb=True)
plt.barbs([0, 1, 2, 3, 4, 5, 6], [0, 0, 0, 0, 0, 0, 0], [0, 3, 8, 15, 40, 65, 120], [0, 1, -2, 5, 10, -30, 0], pivot=1.5, color='C3', linewidth=0.5, flip_barb=[True, False, True, False, True, False, True])

if __name__ == "__main__":
    plt.show()
