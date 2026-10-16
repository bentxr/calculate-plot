# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(7.2, 3))
axs[0].semilogx()
axs[0].boxplot([[1, 3, 4, 6, 9], [2, 4, 4, 5, 7, 12], [0.5, 1, 8]])
axs[1].loglog()
axs[1].boxplot([[1, 3, 4, 6, 90], [2, 4, 4, 5, 7, 12]], orientation='horizontal', positions=[0.5, 20], tick_labels=['a', 'b'])
axs[2].boxplot([[1, 3, 4, 6, 9]])
axs[2].set_xscale('log')

if __name__ == "__main__":
    plt.show()
