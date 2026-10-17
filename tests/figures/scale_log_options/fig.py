# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(8, 6))
axs[0, 0].semilogy([0, 1, 2, 3, 4, 5], [1, 2, 4, 8, 16, 1024], base=2, marker='o')
axs[0, 1].plot([1, 2.718281828459045, 7.38905609893065, 20.085536923187668], [0, 1, 2, 3])
axs[0, 1].set_xscale('log', base=2.718281828459045)
axs[1, 0].loglog([1, 10, 100, 1000], [5, 50, 20, 7000], subs=[2, 5], color='r')
axs[1, 1].plot([0, 1, 2, 3, 4], [-1, 0, 10, 100, 1000], marker='s')
axs[1, 1].set_yscale('log', nonpositive='mask', base=4)
plt.tight_layout()

if __name__ == "__main__":
    plt.show()
