# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(7.0, 3.5))
fig.suptitle('A figure title in $\\mathcal{STIX}$, turned a little', color='C3', style='italic', family='serif', rotation=3, alpha=0.8, math_fontfamily='stix', size=15, weight='light', ha='left', x=0.1)

if __name__ == "__main__":
    plt.show()
