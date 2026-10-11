# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 3, figsize=(11.0, 6.5))
axs[0, 0].errorbar([0, 1, 2, 3], [1, 2, 1.5, 3], yerr=0.3, label='y errors')
axs[0, 0].legend()
axs[0, 1].errorbar([0, 1, 2, 3], [1, 2, 1.5, 3], yerr=[0.1, 0.2, 0.3, 0.4], fmt='o', capsize=4, label='points with caps')
axs[0, 1].plot([0, 3], [1, 3], label='a line, listed first')
axs[0, 1].legend(loc='upper left')
axs[0, 2].errorbar([0, 1, 2, 3], [1, 2, 1.5, 3], xerr=0.2, fmt='none', ecolor='C3', label='bars only, x')
axs[0, 2].errorbar([0, 1, 2, 3], [2, 3, 2.5, 4], xerr=0.2, fmt='none', capsize=3, label='x bars with caps')
axs[0, 2].legend()
axs[1, 0].errorbar([0, 1, 2, 3], [1, 2, 1.5, 3], xerr=0.15, yerr=0.4, fmt='s--', capsize=5, capthick=2, elinewidth=0.8, ecolor='k', color='C2', markersize=9, label='both, squares')
axs[1, 0].legend(fontsize=14)
axs[1, 1].errorbar([0, 1, 2, 3], [1, 2, 1.5, 3], yerr=0.3, alpha=0.5, linewidth=3, label='half there, thick')
axs[1, 1].errorbar([0, 1, 2, 3], [0, 1, 0.5, 2], yerr=0.3, label='_not listed')
axs[1, 1].errorbar([0, 1, 2, 3], [3, 4, 3.5, 5], yerr=0.3, fmt='^', mfc='none', mec='C4', label='open triangles')
axs[1, 1].scatter([0, 1, 2], [5, 5.5, 6], label='points, before the bars')
axs[1, 1].legend(ncols=2, fontsize=7)
axs[1, 2].errorbar([1, 10, 100], [1, 10, 100], yerr=[0.5, 5, 50], xerr=[0.2, 2, 20], fmt='.', label='on log axes')
axs[1, 2].set_xscale('log')
axs[1, 2].set_yscale('log')
axs[1, 2].legend(loc='best', title='Measured')

if __name__ == "__main__":
    plt.show()
