# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.hist([0.5, 1.2, 1.9, 2.2, 2.4, 3.1, 3.3, 3.8, 4.4, 5.0], bins=5, label='hist', alpha=0.6)
plt.bar([1, 3, 5], [2, 1, 2.5], width=0.5, label='bar')
plt.barh([0.5, 1.5], [4, 2], height=0.3, color='C3')
plt.semilogy()
plt.legend()

if __name__ == "__main__":
    plt.show()
