# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import datetime
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'xtick.minor.visible': True,
    'ytick.minor.visible': True,
})
fig, axs = plt.subplots(1, 2, figsize=(8.0, 3.0))
axs[0].scatter([datetime.datetime(1975, 1, 15, 0, 0), datetime.datetime(1975, 1, 15, 0, 1), datetime.datetime(1975, 1, 15, 0, 2), datetime.datetime(1975, 1, 15, 0, 3)], [-1, 1.853, 2, 1.85])
axs[0].set_yscale('symlog', linthresh=1)
axs[1].plot([datetime.datetime(2001, 3, 1, 0, 0), datetime.datetime(2001, 9, 1, 0, 0), datetime.datetime(2002, 6, 1, 0, 0)], [1, 3, 2])

if __name__ == "__main__":
    plt.show()
