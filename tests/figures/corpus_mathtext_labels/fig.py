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
ax1 = fig1.add_axes((0.2, 0.2, 0.75, 0.7))
ax1.plot([1, 2, 3], [1, 4, 9])
ax1.set_xlabel('$\\alpha_i^2 + \\frac{\\beta}{\\gamma}$')
ax1.set_ylabel('$\\sqrt{x} \\cdot \\int_0^\\infty e^{-t} dt$')
ax1.set_title('$\\mathrm{Re}(z) \\leq \\sum_{n=1}^{N} \\hat{x}_n$')

if __name__ == "__main__":
    plt.show()
