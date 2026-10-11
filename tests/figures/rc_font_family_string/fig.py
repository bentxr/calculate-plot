# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'font.family': 'serif, monospace',
    'font.serif': 'no such serif,DejaVu Sans Mono ,',
    'font.monospace': ['', 'DejaVu Serif'],
})
fig, ax = plt.subplots(figsize=(5.0, 3.5))
ax.plot([0, 1, 2], [0, 1, 4], label='in the first family')
ax.set_title('The serif list was given as one string')
ax.set_xlabel('monospace is a serif here', family='monospace')
ax.legend()

if __name__ == "__main__":
    plt.show()
