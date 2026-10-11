# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'axes.formatter.limits': '3, 3',
    'axes.formatter.use_mathtext': True,
    'axes.unicode_minus': False,
    'savefig.bbox': 'tight',
})
fig, axs = plt.subplots(1, 3, figsize=(9.0, 3.0))
axs[0].plot([1, 2, 3, 4], [-1500.0, 2500.0, 1000.0, 7000.0])
axs[1].plot([-1, 2, 3, 4], [-0.5, 0.25, 0.1, 0.7])
axs[2].plot([1000000, 1000001, 1000002], [-1e-07, 1e-07, 2e-07])

if __name__ == "__main__":
    plt.show()
