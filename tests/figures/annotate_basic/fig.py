# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(7.2, 3.6))
axs[0].plot([0, 1, 2], [0, 1, 4], label='line')
axs[0].annotate('peak', (2, 4))
axs[0].annotate('offset', xy=[1, 1], xytext=[10, -15], textcoords='offset points', ha='left')
axs[0].annotate('axes frac', [0.1, 0.9], xycoords='axes fraction', fontsize=12, color='r')
axs[0].annotate('fig px', [20, 20], xycoords='figure pixels')
axs[0].annotate('hidden', [5, 5])
axs[0].annotate('shown', [5, 5], annotation_clip=False)
axs[0].annotate('blend', [1, 0.5], xycoords=('data', 'axes fraction'), xytext=[1, 1], textcoords='offset fontsize', rotation=30)
axs[0].annotate('fig frac', [0.5, 0.02], xycoords='figure fraction', ha='center')
axs[0].annotate('pts', [0, 0], xycoords='axes points', xytext=[30, 40], textcoords='axes pixels')
axs[0].annotate('clip forced', [0.5, 2], xycoords=('axes fraction', 'data'), annotation_clip=True)
axs[0].legend()
axs[1].plot([1, 10, 100], [1, 5, 2])
axs[1].set_xscale('log')
axs[1].annotate('log point', [10, 5], xytext=[30, 4], textcoords='data', va='top')
plt.annotate('pyplot', [50, 2], xytext=[-20, 10], textcoords='offset pixels', fontweight='bold')
axs[1].annotate('', [3, 3])

if __name__ == "__main__":
    plt.show()
