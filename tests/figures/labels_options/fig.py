# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(4.0, 3.0), dpi=120)
ax.set_title('Pulled in', pad=-20)
ax.set_title('right side', loc='right')
ax.set_xlabel('x', loc='right', labelpad=10)
ax.set_ylabel('y (m/s)', loc='top')
ax.set_xlim(5, -5)
ax.set_ylim(top=2.5)

if __name__ == "__main__":
    plt.show()
