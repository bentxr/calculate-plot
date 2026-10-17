# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(8, 6))
axs[0, 0].plot([-100, -10, -1, -0.1, 0, 0.1, 1, 10, 100, 1000], [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], marker='o')
axs[0, 0].set_xscale('symlog')
axs[0, 0].grid()
axs[0, 1].plot([0, 1, 2, 3, 4], [-5000, -20, 0.5, 30, 70000])
axs[0, 1].set_yscale('symlog', linthresh=10, linscale=0.5, subs=[2, 3, 4, 5, 6, 7, 8, 9])
axs[1, 0].scatter([-3, -2, -1, 0, 1, 2, 3], [-8, -4, -1, 0, 1, 4, 8])
axs[1, 0].set_yscale('symlog', base=2, linthresh=1)
axs[1, 0].set_xscale('symlog', linthresh=0.5)
axs[1, 1].fill_between([0, 1, 2, 3], [0, 10, 100, 1000], [-1, -10, -100, -1000], alpha=0.4)
axs[1, 1].set_yscale('symlog')
axs[1, 1].set_ylim(-2000, 2000)
axs[1, 1].axhline(1.5, color='r')
plt.tight_layout()

if __name__ == "__main__":
    plt.show()
