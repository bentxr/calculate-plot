# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.formatter.useoffset': False,
    'axes.formatter.limits': [-2, 3],
    'axes.formatter.min_exponent': 2,
})
fig, axs = plt.subplots(2, 2, figsize=(8.0, 5.0))
axs[0, 0].plot([1, 2, 3, 4], [1234500.0, 1234502.5, 1234501.0, 1234507.0])
axs[0, 1].plot([100, 200, 300, 400], [0.01, 0.03, 0.02, 0.05])
axs[1, 0].plot([-0.5, 0, 0.5, 1], [-500, 300, 999, 1000])
axs[1, 1].plot([0.001, 0.01, 0.1, 1, 10, 100, 1000], [1, 2, 3, 4, 5, 6, 7])
axs[1, 1].set_xscale('log')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
