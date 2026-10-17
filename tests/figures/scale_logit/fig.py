# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(8, 7))
axs[0, 0].plot([0, 1, 2, 3, 4, 5, 6], [0.001, 0.01, 0.1, 0.5, 0.9, 0.99, 0.999], marker='o')
axs[0, 0].set_yscale('logit')
axs[0, 0].grid(which='both')
axs[0, 1].plot([0.2, 0.3, 0.5, 0.7, 0.8], [1, 2, 3, 2, 1])
axs[0, 1].set_xscale('logit', use_overline=True, one_half='1/2')
axs[1, 0].plot([1, 2, 3, 4, 5], [-0.1, 0.02, 0.5, 0.98, 1.2])
axs[1, 0].set_yscale('logit', nonpositive='clip')
axs[1, 1].plot([1, 2, 3], [0.45, 0.5, 0.55])
axs[1, 1].set_yscale('logit')
plt.tight_layout()

if __name__ == "__main__":
    plt.show()
