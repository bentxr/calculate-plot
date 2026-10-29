# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(9.0, 4.0))
axs[0].ecdf([-0.629, 0.622, 1.127, -0.608, -0.043, 0.076, 0.612, 0.628, -0.694, 0.51, -0.688, -0.772, -0.305, -0.2, 1.137, -1.036, -0.547, -1.699, -0.745, -1.116, 0.05, -0.317, -0.124, 0.048, -0.873, 0.361, 1.86, -0.682, -0.572, 0.407], label='plain')
axs[0].ecdf([-0.629, 0.622, 1.127, -0.608, -0.043, 0.076, 0.612, 0.628, -0.694, 0.51, -0.688, -0.772, -0.305, -0.2, 1.137, -1.036, -0.547, -1.699, -0.745, -1.116, 0.05, -0.317, -0.124, 0.048, -0.873, 0.361, 1.86, -0.682, -0.572, 0.407], complementary=True, color='C3', label='complementary')
axs[0].ecdf([-0.629, 0.622, 1.127, -0.608, -0.043, 0.076, 0.612, 0.628, -0.694, 0.51, -0.688, -0.772, -0.305, -0.2, 1.137, -1.036, -0.547, -1.699, -0.745, -1.116, 0.05, -0.317, -0.124, 0.048, -0.873, 0.361, 1.86, -0.682, -0.572, 0.407], [0.96, 1.87, 0.59, 1.36, 0.9, 0.78, 1.08, 1.68, 0.92, 1.45, 0.88, 1.85, 1.02, 1.51, 0.74, 1.06, 0.91, 1.52, 0.69, 1.87, 1.57, 1.98, 1.86, 1.08, 0.55, 0.95, 0.53, 1.42, 1.13, 1.23], linestyle='--')
axs[0].legend()
axs[1].ecdf([3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5], orientation='horizontal', compress=True)
plt.ecdf([3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5], orientation='horizontal', complementary=True, lw=2)

if __name__ == "__main__":
    plt.show()
