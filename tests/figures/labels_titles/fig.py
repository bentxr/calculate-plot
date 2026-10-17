# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.set_title('Measured signal')
ax.set_title('(a)', loc='left')
ax.set_xlabel('time (s)')
ax.set_ylabel('voltage (V)')

if __name__ == "__main__":
    plt.show()
