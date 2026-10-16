# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.prop_cycle': "cycler(color=['r', 'g', 'b', 'k']) + cycler(linestyle=['-', '--', ':', '-.'])",
})
fig, ax = plt.subplots()
ax.hist([[0.0, 2.283, 0.089, -0.952, 2.188, 1.597, -1.1, -0.636, 2.618, 1.993, -1.923, 0.002, 2.6, 0.683, -0.744, 0.718, 2.059, -0.571, -0.557, 2.747, 1.058, -1.519, 0.011, 2.871, 1.281, -1.992, 0.732, 2.481, -0.017, -0.445, 1.34, 1.599, -1.075, 0.032, 3.099, 0.378, -1.696, 0.736, 2.861, 0.558], [2.0, 1.432, 0.159, -0.856, -0.845, 0.184, 1.452, 2.0, 1.413, 0.135, -0.867, -0.834, 0.209, 1.471, 1.999, 1.392, 0.11, -0.877, -0.822, 0.233, 1.49, 1.998, 1.372, 0.086, -0.887, -0.81, 0.258, 1.509, 1.997, 1.351]], histtype='step', bins=5)
ax.fill([-3, -2, -2.5], [0, 0, 3])
ax.hist([-1.0, -0.63, -0.26, 0.11, 0.48, 0.85, 1.22, 1.59, 1.96, 2.33, 2.7, -0.93, -0.56, -0.19, 0.18, 0.55, 0.92, 1.29, 1.66, 2.03, 2.4, 2.77, -0.86, -0.49, -0.12], histtype='stepfilled', bins=4, bottom=[0, 1, 2, 3], stacked=True)
ax.bar([4], [5])
ax.plot([-3, 4], [12, 12])

if __name__ == "__main__":
    plt.show()
