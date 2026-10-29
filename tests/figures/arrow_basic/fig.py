# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(6.4, 4.8))
ax.arrow(0, 0, 1, 0.5)
ax.arrow(0.2, 1.0, 0.8, -0.3, width=0.05, head_width=0.15, head_length=0.2, fc='C1', ec='k', length_includes_head=True)
ax.arrow(1.5, 0.2, -0.6, 0.9, width=0.03, shape='left', overhang=0.4, color='g', alpha=0.6)
ax.arrow(1.8, 1.5, 0, -1.2, width=0.04, shape='right', head_starts_at_zero=True, linewidth=2, hatch='//', facecolor='none')
plt.arrow(0.5, -0.4, 0.7, 0.0, width=0.02, label='plt')

if __name__ == "__main__":
    plt.show()
