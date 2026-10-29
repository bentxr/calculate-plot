# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 3, figsize=(11.0, 6.0))
axs[0, 0].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[0, 0].margins(0.2)
axs[0, 1].scatter([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[0, 1].margins(x=0, y=0.3, tight=False)
axs[0, 2].bar([1, 2, 3], [3, 5, 2])
axs[0, 2].set_xmargin(0.5)
axs[0, 2].set_ymargin(-0.1)
axs[1, 0].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[1, 0].margins(0.05, 0.4, tight=None)
axs[1, 0].locator_params(nbins=4)
axs[1, 1].plot([3.9, 22.099999999999998, 28.6, 63.7, 79.3], [1.1, -0.4, 2.8, 0.9, 3.7])
axs[1, 1].locator_params(axis='y', nbins=3, prune='lower', steps=[2, 5])
axs[1, 1].locator_params(axis='x', integer=True, min_n_ticks=5, symmetric=True, tight=True)
plt.plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
plt.margins(0)
plt.locator_params(nbins='auto', steps=None)

if __name__ == "__main__":
    plt.show()
