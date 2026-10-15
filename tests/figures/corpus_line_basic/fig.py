# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'backend_fallback': False,
})
fig1 = plt.figure(figsize=(3.2, 2.4), dpi=100)
ax1 = fig1.add_axes((0.15, 0.15, 0.8, 0.8))
ax1.plot([0, 1, 2, 3], [0, 1, 4, 9])

if __name__ == "__main__":
    plt.show()
