# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.autolimit_mode': 'round_numbers',
    'axes.xmargin': 0,
    'axes.ymargin': 0,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0.3, 1.7, 4.2], [13, -2.5, 7.75])

if __name__ == "__main__":
    plt.show()
