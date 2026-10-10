# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 1, figsize=(4.0, 5.0))
axs[0].plot([1, 10, 100], [1, 2, 3], label='data')
axs[0].set_xscale('log')
axs[0].legend(loc='upper left')
axs[1].scatter([1, 2, 3], [1, 100, 10000])
axs[1].set_yscale('log')
axs[1].set_ylabel('log y')
fig.tight_layout(rect=(0.05, 0.1, 0.95, 0.9))

if __name__ == "__main__":
    plt.show()
