# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(8.0, 6.0))
axs[0, 0].set_xlabel('red and large', color='red', fontsize='large', fontweight='bold')
axs[0, 0].set_ylabel('level', rotation=0, ha='right', labelpad=10)
axs[0, 1].set_xlabel('placed with x', x=0.2, style='italic')
axs[0, 1].set_ylabel('placed with y', y=0.9, family='monospace')
axs[1, 0].set_xlabel('to the right', loc='right', alpha=0.6, size=8)
axs[1, 0].set_ylabel('at the top', loc='top', c='C4', va='bottom')
axs[1, 1].set_xlabel('ha alone keeps the centre', horizontalalignment='left')
axs[1, 1].set_ylabel('$\\sqrt{x^2+y^2}$\nturned 45', rotation=45, ma='center', fontdict=None)
axs[1, 1].set_xlabel('set again: the colour stays', color='C2')
axs[1, 1].set_xlabel("set again: the colour stays, the place is the loc's again")

if __name__ == "__main__":
    plt.show()
