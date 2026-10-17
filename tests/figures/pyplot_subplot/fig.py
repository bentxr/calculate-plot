# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.figure(figsize=(5.0, 5.0))
plt.subplot(211)
plt.plot([0, 1, 2], [2, 1, 2])
plt.subplot(2, 1, 2)
plt.scatter([0, 1, 2], [1, 2, 1])
plt.ylabel('bottom')
plt.subplot(2, 1, 1)
plt.title('top, again')
ax = plt.gca()
ax.set_ylabel('top')
plt.tight_layout()

if __name__ == "__main__":
    plt.show()
