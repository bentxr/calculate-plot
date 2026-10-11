# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(7.0, 5.0))
ax.set_xlim(0, 10)
ax.set_ylim(0, 10)
ax.grid(True)
ax.text(1, 9, 'vertical', rotation='vertical', va='top')
ax.text(2, 9, 'horizontal', rotation='horizontal')
ax.text(2, 8, 'rotation None', rotation=None)
ax.text(2, 6, '400 degrees', rotation=400)
ax.text(4, 9, 'minus 30', rotation=-30)
ax.text(6, 8, 'spaced\nlines\nof text', linespacing=2.0)
ax.text(8, 8, 'normal\nline\nspacing', linespacing='normal')
ax.text(8, 5.5, 'spacing\nNone', linespacing=None)
ax.text(5, 5, 'centred lines\nin a block\non the right', ma='center', ha='right')
ax.text(5, 3.5, 'lines to the right\nin a block\nat the left', multialignment='right', ha='left')
ax.text(9.5, 0.5, 'ma None\nfollows ha', multialignment=None, ha='right')
ax.text(1, 4, 'anchor mode', rotation=40, rotation_mode='anchor', ha='right', va='top')
ax.text(1, 4, 'default mode', rotation=40, rotation_mode='default', ha='right', va='top', color='C1')
ax.text(1, 2, 'xtick mode', rotation=40, rotation_mode='xtick', color='C2')
ax.text(3, 2, 'ytick mode', rotation=40, rotation_mode='ytick', color='C3')
ax.text(3, 1, 'mode None', rotation=10, rotation_mode=None, va='center_baseline')
ax.text(7, 4, 'top', va='top')
ax.text(7.6, 4, 'bottom', va='bottom')
ax.text(8.6, 4, 'center', va='center')
ax.text(9.4, 4, 'long name', verticalalignment='center', horizontalalignment='center')
ax.text(6.5, 6.5, 'ha wins over the long name', horizontalalignment='left', ha='right', verticalalignment='top', va='bottom', multialignment='left', ma='center')

if __name__ == "__main__":
    plt.show()
