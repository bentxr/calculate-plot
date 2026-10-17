# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2)
axs[0, 0].plot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [0, 1, 4, 9, 16, 25, 36, 49, 64, 81], 'o-', markevery=3, label='every 3')
axs[0, 0].plot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [5, 6, 7, 8, 9, 10, 11, 12, 13, 14], marker='s', markevery=(2, 4))
axs[0, 0].plot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [50, 50, 50, 50, 50, 50, 50, 50, 50, 50], marker='^', markevery=(-2, -3))
axs[0, 0].legend()
axs[0, 1].plot([0, 1, 2, 3, 4, 5], [3, 1, 4, 1, 5, 9], 'x--', markevery=[0, 2, -1])
axs[0, 1].plot([0, 1, 2, 3, 4, 5], [2, 7, 1, 8, 2, 8], 'D:', markevery=[True, False, False, True, True, False])
axs[0, 1].plot([0, 1, 2], [0, 0, 0], 'o', markevery=[])
axs[0, 1].plot([0, 1, 2, 3], [9, 9, 9, 9], 'v', markevery=True)
axs[1, 0].plot([0, 0.1, 0.2, 0.3, 0.5, 0.8, 1.3, 2.1, 3.4, 5.5, 8.9, 14.4], [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1], 'o-', markevery=0.1)
axs[1, 0].plot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14], [2, 2.5, float('nan'), 3, 3.5, 2, 2.5, 3, 2, 2.5, 3, 3.5, 4, 2, 2.5], marker='*', markevery=(0.05, 0.12))
axs[1, 0].plot([0, 2, 4, 6, 8, 10, 12, 14], [-1, -1, -1, -1, -1, -1, -1, -1], marker='o', markevery=(0, 0.3), drawstyle='steps-mid')
axs[1, 1].loglog([1, 2, 5, 10, 20, 50, 100, 200, 500, 1000], [1, 3, 9, 27, 81, 243, 729, 2187, 6561, 19683], marker='o', markevery=0.2)
axs[1, 1].plot([1, 1000], [10, 10], marker='o', markevery=(1, 1.5))

if __name__ == "__main__":
    plt.show()
