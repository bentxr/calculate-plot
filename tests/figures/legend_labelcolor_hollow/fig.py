# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'markers.fillstyle': 'none',
    'legend.labelcolor': 'markerfacecolor',
})
fig, axs = plt.subplots(1, 3, figsize=(10.0, 3.5))
axs[0].plot([1, 2, 3], [1, 2, 1], 'o-', label='hollow markers', color='C1')
axs[0].errorbar([1, 2, 3], [2, 3, 2], yerr=0.3, xerr=0.2, label='both bars', fmt='s', color='C2', ecolor='C3')
axs[0].vlines([1.5, 2.5], 0, 3, label='wide lines', colors='C4', linewidths=4, linestyles='dashed')
axs[0].axvspan(1.8, 2.2, label='span', color='C5', alpha=0.4)
axs[0].legend()
axs[1].plot([1, 2, 3], [1, 2, 1], 'o-', label='hollow markers', color='C1')
axs[1].errorbar([1, 2, 3], [2, 3, 2], yerr=0.3, xerr=0.2, label='both bars', fmt='s', color='C2', ecolor='C3')
axs[1].vlines([1.5, 2.5], 0, 3, label='wide lines', colors='C4', linewidths=4, linestyles='dashed')
axs[1].axvspan(1.8, 2.2, label='span', color='C5', alpha=0.4)
axs[1].errorbar([1, 2, 3], [2.5, 3.5, 2.5], yerr=0.3, xerr=0.2, label='more bars', fmt='none', ecolor='C7')
axs[1].legend(labelcolor='linecolor')
axs[2].plot([1, 2, 3], [1, 2, 1], 'o-', label='hollow markers', color='C1', markeredgecolor='C6')
axs[2].errorbar([1, 2, 3], [2, 3, 2], yerr=0.3, xerr=0.2, label='both bars', fmt='s', color='C2', ecolor='C3')
axs[2].vlines([1.5, 2.5], 0, 3, label='wide lines', colors='C4', linewidths=4, linestyles='dashed')
axs[2].axvspan(1.8, 2.2, label='span', color='C5', alpha=0.4)
axs[2].legend(labelcolor='mec')

if __name__ == "__main__":
    plt.show()
