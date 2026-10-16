# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'hist.bins': 'doane',
})
fig, axs = plt.subplots(1, 2)
axs[0].hist([1.0, 3.079, 2.245, -0.178, -0.642, 1.451, 2.912, 0.074, -2.245, -2.063, 0.441, 1.809, 0.378, -1.289, -1.574, 1.349, 2.503, 0.912, -0.411, 1.085, 3.722, 2.629, 0.419, -0.935, 0.574, 2.672, 2.17, -0.326, -2.662, -0.705, 1.335, 0.647, -1.457, -1.459, 1.192, 1.852, 1.0, -0.8, -0.216, 2.531, 3.959, 2.403, -0.96, -0.286, 2.089, 2.643, 0.458, -1.394, -0.313, 0.657, 0.875, -1.236, -2.313, -0.377, 2.255, 2.346, -0.953, -1.345, 1.049, 3.376])
axs[1].hist([[0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 0, 4, 8, 1, 5, 9], [3, 1, 4, 1, 5, 9, 2, 6]], histtype='step')

if __name__ == "__main__":
    plt.show()
