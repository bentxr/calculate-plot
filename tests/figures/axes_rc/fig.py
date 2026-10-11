# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'patch.force_edgecolor': True,
    'axes.facecolor': '#eeeeee',
    'axes.edgecolor': 'C3',
    'axes.linewidth': 1.5,
    'xtick.direction': 'in',
    'ytick.direction': 'inout',
    'xtick.top': True,
    'ytick.right': True,
    'xtick.labeltop': True,
    'ytick.labelright': True,
    'axes.spines.top': False,
    'xtick.major.size': 6.0,
    'ytick.major.width': 2.0,
    'xtick.color': 'tab:blue',
    'ytick.labelcolor': 'green',
    'figure.facecolor': '0.9',
    'font.size': 12.0,
    'xtick.labelsize': 'small',
    'figure.subplot.left': 0.2,
    'figure.subplot.top': 0.8,
    'xtick.major.pad': 6.0,
    'ytick.alignment': 'center',
})
fig, ax = plt.subplots(figsize=(4.0, 3.0), dpi=80)

if __name__ == "__main__":
    plt.show()
