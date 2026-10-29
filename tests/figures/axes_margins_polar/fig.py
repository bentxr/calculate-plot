# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, subplot_kw={'projection': 'polar'}, figsize=(8.0, 4.0))
axs[0].plot([0.2, 0.9, 1.7, 2.5], [1.0, 2.5, 1.7, 3.2])
axs[0].margins(0.3)
axs[0].locator_params(nbins=3)
axs[1].plot([0.2, 0.9, 1.7, 2.5], [1.0, 2.5, 1.7, 3.2])
axs[1].set_ymargin(0.25)
axs[1].set_thetamax(180)

if __name__ == "__main__":
    plt.show()
