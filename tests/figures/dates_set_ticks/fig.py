# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import datetime
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(8.0, 3.0))
axs[0].plot([datetime.datetime(1975, 6, 15, 0, 0), datetime.datetime(1977, 8, 23, 0, 0), datetime.datetime(1979, 11, 1, 0, 0)], [1, 3, 2])
axs[0].set_xticks([2000, 2500, 3000, 3500])
axs[1].plot([datetime.datetime(2020, 1, 1, 10, 0), datetime.datetime(2020, 1, 1, 10, 30)], [1, 2])
axs[1].set_xticks([18262.42, 18262.43])

if __name__ == "__main__":
    plt.show()
