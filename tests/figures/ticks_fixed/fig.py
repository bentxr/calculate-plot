# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.2, 5.4))
axs[0, 0].plot([0, 1, 2], [1, 3, 2])
axs[0, 0].set_xticks([0, 0.5, 2.5])
axs[0, 0].set_yticks([1, 2, 3], ['one', 'two', 'three'])
axs[0, 1].plot([0, 1], [100000, 100002])
axs[0, 1].set_yticks([100000, 100001, 100002])
axs[0, 1].set_xticks([])
axs[1, 0].plot([1, 10, 100], [1, 2, 3])
axs[1, 0].set(xscale='log', xticks=[1, 30, 100], frame_on=False)
axs[1, 1].plot([0, 1], [0, 1])
axs[1, 1].set_xticks([0.2, 0.4], labels=[])
axs[1, 1].set_frame_on(False)
axs[1, 1].plot([0, 2], [0, 2])

if __name__ == "__main__":
    plt.show()
