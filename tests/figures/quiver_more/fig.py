# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(11.0, 3.8))
q1 = axs[0].quiver([0, 1, 2, 3, 4, 5], [0, 1, 2], [[1.0, 2.0, float('nan'), 0.05, 0.3, 1.5], [0.5, -1, 1, 0.0, 2, 1], [1, 1, 1, 1, 1, 1]], [[0.0, 1.0, 1.0, 0.02, 0.3, -1.5], [0.5, 1, -1, 0.001, 0, 1], [0, 0.5, 1, 1.5, 2, 2.5]], units='inches', scale_units='inches', scale=2, color=['r', 'g', 'b'], minlength=2)
axs[0].quiverkey(q1, 2, 2.6, 1, '1 in', coordinates='data', labelpos='S', color='m', labelcolor='C1')
q2 = axs[1].quiver([0, 1, 2, 3, 4, 5], [0, 1, 2], [[1.0, 2.0, float('nan'), 0.05, 0.3, 1.5], [0.5, -1, 1, 0.0, 2, 1], [1, 1, 1, 1, 1, 1]], [[0.0, 1.0, 1.0, 0.02, 0.3, -1.5], [0.5, 1, -1, 0.001, 0, 1], [0, 0.5, 1, 1.5, 2, 2.5]], angles=[0, 15, 30, 45, 60, 75, 90, 105, 120, 135, 150, 165, 180, 195, 210, 225, 240, 255], units='dots', width=3, scale=0.02, headaxislength=3, minshaft=2)
axs[1].quiverkey(q2, 0.3, 1.05, 1, 'key', labelpos='W', angle=30, fontproperties={'size': 8, 'weight': 'bold'}, labelsep=0.2)
q3 = plt.quiver([0, 1, 2, 3, 4, 5], [0, 1, 2], [[1.0, 2.0, float('nan'), 0.05, 0.3, 1.5], [0.5, -1, 1, 0.0, 2, 1], [1, 1, 1, 1, 1, 1]], [[0.0, 1.0, 1.0, 0.02, 0.3, -1.5], [0.5, 1, -1, 0.001, 0, 1], [0, 0.5, 1, 1.5, 2, 2.5]], [[1, 2, 3, 4, 5, 6], [2, 3, 4, 5, 6, 7], [3, 4, 5, 6, 7, 8]], cmap='plasma', scale_units='xy', angles='xy', alpha=0.7)
plt.quiverkey(q3, 0.5, 0.5, 1, 'N', coordinates='figure', labelpos='N')
plt.colorbar()

if __name__ == "__main__":
    plt.show()
