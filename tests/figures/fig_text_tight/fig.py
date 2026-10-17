# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'savefig.bbox': 'tight',
})
fig, ax = plt.subplots(figsize=(5.0, 3.0))
ax.plot([0, 1, 2], [0, 1, 4])
fig.text(1.1, 0.5, 'beyond the right edge', fontsize=14)
fig.text(0.5, -0.1, 'below the figure', ha='center')
fig.text(0.5, 1.5, 'hidden far above', visible=False)
fig.text(0.2, 1.2, '')

if __name__ == "__main__":
    plt.show()
