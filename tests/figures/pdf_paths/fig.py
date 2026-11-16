# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(figsize=(5.0, 2.5))
ax = fig.add_subplot(1, 2, 1)
pax = fig.add_subplot(1, 2, 2, projection='polar')
ax.plot([0, 1, 2, 3], [0, 2, 1, 3], linewidth=4, solid_capstyle='round', solid_joinstyle='miter')
ax.plot([0, 1, 2, 3], [3, 1, 2, 0], linestyle='--', color='0.4', alpha=0.5)
ax.plot([0, 3], [1.5, 1.5], linestyle='-.', color=[0.2, 0.6, 0.3, 0.7], dash_capstyle='projecting')
ax.plot([-1, 4], [-1, 4], linestyle=':', color='C3', linewidth=0.5)
ax.fill([0.5, 1.5, 1.0], [0.5, 0.5, 2.5], facecolor='C4', edgecolor='k', alpha=0.6)
ax.fill([2.0, 2.8, 2.8, 2.0], [2.0, 2.0, 2.8, 2.8], fill=False, edgecolor='C5', linewidth=2)
ax.fill([2.0, 3.0, 2.5], [0.0, 0.0, 0.8], facecolor=[0.5, 0.5, 0.5], linewidth=0)
ax.plot([0.2, 0.6, 1.0], [2.9, 2.2, 2.9], linewidth=5, solid_joinstyle='bevel', color='0')
ax.set_xlim(0, 3)
ax.set_ylim(0, 3)
ax.axis('off')
pax.plot([0, 1, 2, 3, 4, 5, 6], [1, 2, 3, 4, 5, 6, 7], color='C0', linewidth=3)
pax.set_rmax(5)
pax.axis('off')

if __name__ == "__main__":
    plt.show()
