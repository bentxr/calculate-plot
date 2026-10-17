# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(3, 1, figsize=(4.0, 6.0))
axs[0].set_title('first')
axs[1].set_xlabel('second')
axs[2].plot([0, 1, 2], [0.0001, 0.0002, 0.0004], 'o-', markersize=20)
fig.tight_layout(h_pad=2.5)
fig.subplots_adjust(top=0.8)

if __name__ == "__main__":
    plt.show()
