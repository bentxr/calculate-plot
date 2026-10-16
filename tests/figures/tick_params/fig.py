# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(8, 6))
axs[0, 0].plot([0, 1, 2, 3], [3, 1, 4, 1])
axs[0, 1].plot([0, 1, 2, 3], [3, 1, 4, 1])
axs[1, 0].plot([0, 1, 2, 3], [3, 1, 4, 1])
axs[1, 1].plot([0, 1, 2, 3], [3, 1, 4, 1])
axs[0, 0].tick_params(direction='in', length=8, width=2, colors='r', pad=8)
axs[0, 0].tick_params(axis='y', labelsize='small', labelcolor='C2', rotation=30)
axs[0, 1].tick_params(top=True, labeltop=True, bottom=False, labelbottom=False, right=True, labelright=True, labelleft=False)
axs[0, 1].tick_params(which='both', direction='inout', grid_color='0.5', grid_alpha=0.5)
axs[0, 1].grid()
axs[1, 0].semilogy()
axs[1, 0].tick_params(which='minor', length=6, color='b', width=1.5)
axs[1, 0].tick_params(axis='x', labelrotation=45, labelsize=12, zorder=0)
axs[1, 1].tick_params(axis='both', which='major', labelfontfamily='serif', gridOn=True, grid_linestyle=':', grid_lw=2)
axs[1, 1].yaxis.set_tick_params(which='major', labelleft=False, labelright=True, tick2On=True, rotation_mode='anchor', labelrotation=-20)
axs[1, 1].xaxis.set_tick_params(reset=True, length=2)
plt.tight_layout()

if __name__ == "__main__":
    plt.show()
