# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.plot([0, 1, 2, 3], [0, 1, 4, 9], label='squares')
plt.plot([0, 1, 2, 3], [0, 1, 2, 3], 'o--', label='line')
plt.title('implicit axes')
plt.xlabel('x')
plt.ylabel('y')
plt.xlim(-0.5, 3.5)
plt.axhline(5, color='C3')
plt.grid(True)
plt.text(0.2, 7, 'a note')
plt.legend(loc='upper left')
plt.suptitle('pyplot')

if __name__ == "__main__":
    plt.show()
