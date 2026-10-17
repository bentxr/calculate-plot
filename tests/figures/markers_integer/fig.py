# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(7.2, 3.6))
axs[0].plot([0, 1, 2], [0, 0.4, 0], marker=0, markersize=9, label='0')
axs[0].plot([0, 1, 2], [1, 1.4, 1], marker=1, markersize=9, label='1')
axs[0].plot([0, 1, 2], [2, 2.4, 2], marker=2, markersize=9, label='2')
axs[0].plot([0, 1, 2], [3, 3.4, 3], marker=3, markersize=9, label='3')
axs[0].plot([0, 1, 2], [4, 4.4, 4], marker=4, markersize=9, label='4')
axs[0].plot([0, 1, 2], [5, 5.4, 5], marker=5, markersize=9, label='5')
axs[0].plot([0, 1, 2], [6, 6.4, 6], marker=6, markersize=9, label='6')
axs[0].plot([0, 1, 2], [7, 7.4, 7], marker=7, markersize=9, label='7')
axs[0].plot([0, 1, 2], [8, 8.4, 8], marker=8, markersize=9, label='8')
axs[0].plot([0, 1, 2], [9, 9.4, 9], marker=9, markersize=9, label='9')
axs[0].plot([0, 1, 2], [10, 10.4, 10], marker=10, markersize=9, label='10')
axs[0].plot([0, 1, 2], [11, 11.4, 11], marker=11, markersize=9, label='11')
axs[0].legend(fontsize=6, ncols=2, loc='upper left')
axs[1].scatter([0, 1, 2], [0, 0, 0], marker=4, s=120, label='4')
axs[1].scatter([0, 1, 2], [1, 1, 1], marker=5.0, s=120, label='5.0')
axs[1].scatter([0, 1, 2], [2, 2, 2], marker=True, s=120, label='True')
axs[1].scatter([0, 1, 2], [3, 3, 3], marker=False, s=120, label='False')
axs[1].scatter([0, 1, 2], [4, 4, 4], marker=10, s=120, label='10')
axs[1].scatter([0, 1, 2], [5, 5, 5], marker=11, s=120, label='11')
axs[1].legend(fontsize=6)

if __name__ == "__main__":
    plt.show()
