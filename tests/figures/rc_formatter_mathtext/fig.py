# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.formatter.use_mathtext': 'yes',
    'axes.formatter.offset_threshold': 2,
    'axes.formatter.min_exponent': 3,
})
fig, axs = plt.subplots(2, 2, figsize=(8.0, 5.0))
axs[0, 0].plot([1, 2, 3, 4], [-1234500.0, -1234502.5, -1234501.0, -1234507.0])
axs[0, 1].plot([100, 101, 102, 103], [-3e-07, 1e-07, 2e-07, 5e-07])
axs[1, 0].plot([-0.5, 0, 0.5, 1], [250000000.0, 300000000.0, 999000000.0, 1000000000.0])
axs[1, 1].plot([0.001, 0.01, 0.1, 1, 10, 100, 1000, 10000], [0.2, 0.3, 0.5, 0.9, 2, 4, 6, 7])
axs[1, 1].set_xscale('log')
axs[1, 1].set_yscale('log')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
