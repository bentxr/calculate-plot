# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.2, 5.4))
axs[0, 0].stem([1, 2, 3, 4], [1, -2, 3, 0.5], label='stem')
axs[0, 0].legend()
axs[0, 1].stem([3, 1, 2], linefmt='r--', markerfmt='gs', basefmt='k:', bottom=0.5, label='dashed')
axs[0, 1].legend(numpoints=2)
axs[1, 0].stem([0, 1, 2], [1, 2, 1.5], 'C2-.', orientation='horizontal', markerfmt='', label='h')
axs[1, 0].legend()
axs[1, 1].stem([1, 2], [3, 4], 'o')
axs[1, 1].stem([3, 4], [2, 1], markerfmt='D', basefmt=' ', linefmt='m')
axs[1, 1].stem([5, 6], [1, 2], linefmt='r', markerfmt='k^:')

if __name__ == "__main__":
    plt.show()
