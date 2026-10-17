# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(7.2, 3.2))
axs[0].plot([0, 1, 2, 3.5, 4, 5, 7], [1.0, 3.0, 2.0, 2.5, float('nan'), 1.0, 2.0], drawstyle='default', marker='o', label='default')
axs[0].plot([0, 1, 2, 3.5, 4, 5, 7], [3.5, 5.5, 4.5, 5.0, float('nan'), 3.5, 4.5], drawstyle='steps', marker='o', label='steps')
axs[0].plot([0, 1, 2, 3.5, 4, 5, 7], [6.0, 8.0, 7.0, 7.5, float('nan'), 6.0, 7.0], drawstyle='steps-pre', marker='o', label='steps-pre')
axs[0].plot([0, 1, 2, 3.5, 4, 5, 7], [8.5, 10.5, 9.5, 10.0, float('nan'), 8.5, 9.5], drawstyle='steps-mid', marker='o', label='steps-mid')
axs[0].plot([0, 1, 2, 3.5, 4, 5, 7], [11.0, 13.0, 12.0, 12.5, float('nan'), 11.0, 12.0], drawstyle='steps-post', marker='o', label='steps-post')
axs[0].plot([0, 1, 2, 3.5, 4, 5, 7], [13.5, 15.5, 14.5, 15.0, float('nan'), 13.5, 14.5], drawstyle=None, marker='o', label='None')
axs[0].legend(fontsize=7, numpoints=1)
axs[1].plot([1, 2, 3, 4], [1, 4, 2, 3], ds='steps-mid', marker='s', label='two points')
axs[1].errorbar([1, 2, 3, 4], [3, 6, 4, 5], yerr=0.4, fmt='^-', drawstyle='steps-post', label='errorbar')
axs[1].axhline(2.5, drawstyle='steps-mid', color='C5')
axs[1].legend(numpoints=2)

if __name__ == "__main__":
    plt.show()
