# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'xtick.labeltop': True,
    'xtick.top': True,
    'xtick.major.top': False,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([2, 3, 6], [0, 3, 1])
ax.set_xscale('log')
ax.set_title('Minor labels on top only')

if __name__ == "__main__":
    plt.show()
