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
ax1 = fig1.add_axes((0.12, 0.12, 0.83, 0.83))
ax1.plot([0, 1], [0, 1], label='series 0', marker='o')
ax1.plot([0, 1], [1, 2], label='series 1', marker='s')
ax1.plot([0, 1], [2, 3], label='series 2', marker='^')
ax1.legend(title='Legend', shadow=True, fancybox=True, framealpha=0.7)

if __name__ == "__main__":
    plt.show()
