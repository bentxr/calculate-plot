# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(9.0, 7.0))
axs[0, 0].xcorr([-0.198, 0.445, 0.645, 1.285, 0.612, 0.951, 0.64, 0.781, -0.202, -0.11, -0.776, -0.975, -0.634, -1.218, -0.796, -0.554, 0.312, 0.696, 1.141, 1.056, 1.356, 0.617, 1.025, 0.233, -0.205, -0.703, -1.365, -0.886, -0.516, -0.909, -0.666, -0.37, 0.153, 0.971, 1.047, 1.056, 0.575, 0.676, 1.015, -0.183], [0.989, 0.891, 0.631, 0.852, -0.27, -0.674, -0.727, -1.089, -0.891, -0.968, 0.182, -0.474, -0.297, 1.018, 0.616, 1.238, 1.472, 1.512, 0.838, 0.764, -0.091, -0.903, -0.699, -1.361, -0.719, -0.571, -0.665, -0.577, 0.545, 0.658, 1.028, 0.746, 0.828, 0.855, 0.493, -0.037, -0.025, -0.536, -0.722, -1.131], usevlines=True, maxlags=15, lw=2, label='xc')
axs[0, 0].legend()
axs[0, 1].acorr([-0.198, 0.445, 0.645, 1.285, 0.612, 0.951, 0.64, 0.781, -0.202, -0.11, -0.776, -0.975, -0.634, -1.218, -0.796, -0.554, 0.312, 0.696, 1.141, 1.056, 1.356, 0.617, 1.025, 0.233, -0.205, -0.703, -1.365, -0.886, -0.516, -0.909, -0.666, -0.37, 0.153, 0.971, 1.047, 1.056, 0.575, 0.676, 1.015, -0.183], usevlines=False, normed=False, color='C2')
axs[1, 0].acorr([0.597, -1.555, 1.199, 0.267, 0.328, 0.264, -0.481, -0.863, 0.231], maxlags=None)
plt.xcorr([1, 3, 2, 5, 4, 6, 2, 1], [1, 2, 6, 4, 5, 2, 3, 1], maxlags=5, usevlines=False, marker='s')

if __name__ == "__main__":
    plt.show()
