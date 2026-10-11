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
fig, ax = plt.subplots(figsize=(5.0, 3.5))
ax.plot([0, 1, 2], [0, 1, 4])
ax.text(2.0, 4.0, 'this text reaches far out to the right of the figure', fontsize=14)
ax.text(-0.4, -1.5, 'and this one below, to the left', rotation=20)
ax.text(1.0, 9.0, 'clipped: it does not count', clip_on=True)
ax.text(1.0, 12.0, 'hidden: it does not count', visible=False)
ax.text(-5.0, 2.0, '')
ax.text(float('nan'), 2.0, 'nowhere')

if __name__ == "__main__":
    plt.show()
