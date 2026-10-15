# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, sharex='col', figsize=(6.4, 4.8))
axs[0, 0].plot([1, 2, 3, 4], [100000.0, 100002.0, 100001.0, 100004.0])
axs[0, 0].set_title('offset text gone')
axs[0, 0].yaxis.set_major_formatter(plt.NullFormatter())
axs[1, 0].plot([1, 2, 3, 4], [3, 1, 4, 1])
axs[1, 0].xaxis.set_major_formatter(plt.NullFormatter())
axs[0, 1].plot([1, 10, 100, 1000], [2, 30, 200, 5000])
axs[0, 1].set_yscale('log')
axs[0, 1].yaxis.set_minor_formatter(plt.NullFormatter())
axs[1, 1].plot([1, 2, 3], [0.5, 0.7, 0.2])
axs[1, 1].yaxis.set_major_formatter(plt.NullFormatter())
axs[1, 1].set_yscale('log')
axs[1, 1].set_ylabel("the scale's own formatter again")
fig.suptitle('')
axs[1, 1].set_title('')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
