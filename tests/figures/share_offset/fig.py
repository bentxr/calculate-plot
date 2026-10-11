# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, sharex=True, sharey=True, figsize=(6.0, 4.0))
axs[0, 0].plot([10000, 10001], [1000000, 3000000])
axs[1, 1].plot([10000.5, 10002], [2000000, 5000000])
axs[0, 1].grid(True)

if __name__ == "__main__":
    plt.show()
