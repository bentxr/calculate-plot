# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(7.0, 3.5))
axs[0].plot([0, 1, 2, 3], [0, 1, 4, 9], label='squares')
axs[0].errorbar([0, 1, 2, 3], [1, 2, 3, 2], yerr=0.4, label='measured', capsize=3)
axs[0].plot([0, 3], [5, 5], label='_hidden')
axs[0].plot([0, 3], [7, 2], label='falling', linestyle='--', marker='o')
axs[1].bar([1, 2, 3], [3, 1, 2], label='bars', color='C2')
axs[1].scatter([1, 2, 3], [2.5, 2, 0.5], label='points', color='C3')
axs[1].fill_between([1, 2, 3], [0.2, 0.4, 0.3], label='band', alpha=0.4)
fig.legend()
fig.legend(loc='lower center', ncols=3, title='All of them', fontsize='small')
fig.legend(loc=[0.01, 0.6], frameon=False, fontsize=7)
fig.legend(loc=10, fancybox=False, facecolor='lightyellow', markerfirst=False)

if __name__ == "__main__":
    plt.show()
