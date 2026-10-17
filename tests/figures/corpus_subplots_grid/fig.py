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
fig1, axs1 = plt.subplots(2, 2, figsize=(4, 3), dpi=100)
axs1[0, 0].plot([0, 1, 2, 3, 4, 5, 6, 7, 8, 9])
axs1[0, 0].set_title('panel 0', fontsize=8)
axs1[0, 1].plot([0, 1, 4, 9, 16, 25, 36, 49, 64, 81])
axs1[0, 1].set_title('panel 1', fontsize=8)
axs1[1, 0].plot([0, 1, 8, 27, 64, 125, 216, 343, 512, 729])
axs1[1, 0].set_title('panel 2', fontsize=8)
axs1[1, 1].plot([0, 1, 16, 81, 256, 625, 1296, 2401, 4096, 6561])
axs1[1, 1].set_title('panel 3', fontsize=8)
fig1.suptitle('Grid')
fig1.tight_layout()

if __name__ == "__main__":
    plt.show()
