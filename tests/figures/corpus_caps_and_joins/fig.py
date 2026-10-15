# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'backend_fallback': False,
})
fig1 = plt.figure(figsize=(3.2, 2.4), dpi=100)
ax1 = fig1.add_axes((0.05, 0.05, 0.9, 0.9))
ax1.set_axis_off()
ax1.plot([0, 1, 2], [0, 1.5, 0], linewidth=8, solid_capstyle='butt', solid_joinstyle='miter', color='C0')
ax1.plot([0, 1, 2], [2, 3.5, 2], linewidth=8, solid_capstyle='butt', solid_joinstyle='round', color='C3')
ax1.plot([0, 1, 2], [4, 5.5, 4], linewidth=8, solid_capstyle='butt', solid_joinstyle='bevel', color='C6')
ax1.plot([3, 4, 5], [0, 1.5, 0], linewidth=8, solid_capstyle='round', solid_joinstyle='miter', color='C1')
ax1.plot([3, 4, 5], [2, 3.5, 2], linewidth=8, solid_capstyle='round', solid_joinstyle='round', color='C4')
ax1.plot([3, 4, 5], [4, 5.5, 4], linewidth=8, solid_capstyle='round', solid_joinstyle='bevel', color='C7')
ax1.plot([6, 7, 8], [0, 1.5, 0], linewidth=8, solid_capstyle='projecting', solid_joinstyle='miter', color='C2')
ax1.plot([6, 7, 8], [2, 3.5, 2], linewidth=8, solid_capstyle='projecting', solid_joinstyle='round', color='C5')
ax1.plot([6, 7, 8], [4, 5.5, 4], linewidth=8, solid_capstyle='projecting', solid_joinstyle='bevel', color='C8')

if __name__ == "__main__":
    plt.show()
