# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(3, 2, sharex='col', sharey=True, figsize=(5.0, 5.0))
axs[0, 0].plot([0, 1, 2], [0, 1000, 4000])
axs[2, 1].errorbar([1, 2, 3], [100, 200, 150], yerr=50)
axs[1, 0].set_ylabel('middle')
axs[2, 0].set_xlabel('left column')
fig.suptitle('Shared')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
