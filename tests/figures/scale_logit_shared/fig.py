# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 1, sharey=True, figsize=(4.0, 5.0))
axs[0].plot([1, 2, 3], [0.4, 0.6, 0.9])
axs[1].plot([1, 2, 3, 4], [0.001, 0.5, 0.3, 1.5])
axs[1].set_yscale('logit')

if __name__ == "__main__":
    plt.show()
