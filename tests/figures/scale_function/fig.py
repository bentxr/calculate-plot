# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt
import numpy as np

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(9.0, 7.0))
axs[0, 0].plot([0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100], [0.0, 2.23606797749979, 3.1622776601683795, 3.872983346207417, 4.47213595499958, 5.0, 5.477225575051661, 5.916079783099616, 6.324555320336759, 6.708203932499369, 7.0710678118654755, 7.416198487095663, 7.745966692414834, 8.06225774829855, 8.366600265340756, 8.660254037844387, 8.94427190999916, 9.219544457292887, 9.486832980505138, 9.746794344808963, 10.0], marker='o')
axs[0, 0].set_xscale('function', functions=(lambda x: x ** 0.5, lambda x: x ** 2))
axs[0, 1].plot([1, 2, 5, 10, 20, 50, 100], [3, 1, 4, 1, 5, 9, 2])
axs[0, 1].set_yscale('functionlog', functions=[lambda x: x * 10, lambda x: x / 10])
axs[1, 0].plot([-50, -10, -1, 0, 1, 10, 50], [0, 1, 2, 3, 2, 1, 0])
axs[1, 0].set_xscale('function', functions=(lambda x: np.arcsinh(x), lambda x: np.sinh(x)))
axs[1, 0].set_title('arcsinh')
axs[1, 1].scatter([0.5, 1, 2, 4, 8, 16], [1, 3, 2, 5, 4, 6])
axs[1, 1].set_xscale('functionlog', functions=(lambda x: x * 2 + 1, lambda x: (x - 1) / 2), base=2)
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
