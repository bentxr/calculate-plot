# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, layout='constrained', figsize=(6.0, 4.0))
axs[0].plot([0, 1, 2], [0, 1, 4], label='growing')
axs[1].plot([0, 1, 2], [4, 1, 0], label='shrinking')
fig.suptitle('Outside legends')
fig.legend(loc='outside right upper')
fig.legend(loc='outside lower center', ncols=2)
fig.legend(loc='outside upper center', ncols=2)
fig.legend(loc='outside left center', title='left')
fig.legend(loc='outside center', fontsize=6)

if __name__ == "__main__":
    plt.show()
