# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'font.fantasy': 'DejaVu Serif, fantasy',
    'font.cursive': ['DejaVu Sans Mono'],
    'font.monospace': 'no such font,  DejaVu Sans Mono ,',
})
fig, ax = plt.subplots(figsize=(5.0, 3.0))
ax.text(0.1, 0.85, 'fantasy: efficient', family='fantasy')
ax.text(0.1, 0.7, 'cursive: efficient', family='cursive')
ax.text(0.1, 0.55, 'monospace: efficient', family='monospace')
ax.text(0.1, 0.4, 'serif: efficient', family='serif')
ax.text(0.1, 0.25, 'sans-serif: efficient', family='sans-serif')
ax.text(0.1, 0.1, 'sans: efficient', family=['sans', 'fantasy'])

if __name__ == "__main__":
    plt.show()
