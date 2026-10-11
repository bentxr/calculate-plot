# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.unicode_minus': False,
})
fig, axs = plt.subplots(2, 2, figsize=(7.0, 5.0))
axs[0, 0].plot([-3, -1, 1, 3], [-2.5, -0.5, 1.5, 3.5])
axs[0, 1].plot([1, 2, 3, 4], [-1234500.0, -1234502.5, -1234501.0, -1234507.0])
axs[1, 0].plot([1e-09, 2e-09, 3e-09], [-1e-07, -3e-07, -2e-07])
axs[1, 1].plot([0.001, 0.01, 0.1, 1], [-5, -1, 0, 4])
axs[1, 1].set_xscale('log')
axs[1, 1].set_title('a title - not a minus; $-x$')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
