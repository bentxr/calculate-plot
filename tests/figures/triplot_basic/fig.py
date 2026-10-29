# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt
import matplotlib.tri as mtri

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(11.0, 3.6))
axs[0].triplot([2.276, 1.64, 2.336, 0.048, 3.068, 1.962, 1.138, 0.95, 1.55, 3.686, 3.591, 3.017, 0.145, 1.031, 1.256, 0.065, 3.095, 3.912, 1.085, 1.018], [0.563, 0.925, 1.579, 2.103, 0.205, 0.309, 0.861, 1.904, 2.041, 2.661, 0.633, 0.3, 0.792, 0.07, 1.526, 1.395, 2.957, 2.767, 0.097, 1.569])
axs[1].triplot([0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0, 3.0], [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 2.0, 2.0, 2.0, 2.0], [[0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 5, 6], [2, 3, 7], [2, 7, 6], [4, 5, 9], [4, 8, 9], [5, 6, 10], [5, 10, 9], [6, 7, 11], [6, 10, 11]], 'go-', lw=1.0, markersize=4)
t = mtri.Triangulation([0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0, 3.0], [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 2.0, 2.0, 2.0, 2.0], [[0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 5, 6], [2, 3, 7], [2, 7, 6], [4, 5, 9], [4, 8, 9], [5, 6, 10], [5, 10, 9], [6, 7, 11], [6, 10, 11]], mask=[False, True, False, False, False, True, False, False, False, True, False, False])
axs[2].triplot(t, color='C3', marker='.', label='mesh')
axs[2].legend()
plt.triplot([2.276, 1.64, 2.336, 0.048, 3.068, 1.962, 1.138, 0.95, 1.55, 3.686, 3.591, 3.017, 0.145, 1.031, 1.256, 0.065, 3.095, 3.912, 1.085, 1.018], [0.563, 0.925, 1.579, 2.103, 0.205, 0.309, 0.861, 1.904, 2.041, 2.661, 0.633, 0.3, 0.792, 0.07, 1.526, 1.395, 2.957, 2.767, 0.097, 1.569], linestyle=':', alpha=0.4)

if __name__ == "__main__":
    plt.show()
