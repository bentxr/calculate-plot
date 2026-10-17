# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'xtick.labeltop': True,
    'xtick.labelbottom': False,
    'xtick.top': True,
    'xtick.bottom': False,
})
fig, axs = plt.subplots(1, 2, figsize=(6.0, 3.0))
axs[0].plot([10000, 10001], [0, 1])
axs[0].set_title('Above the top labels')
axs[1].set_title('Left', loc='left')
axs[1].set_title('Right', loc='right')

if __name__ == "__main__":
    plt.show()
