# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'xtick.labelbottom': False,
    'ytick.labelleft': False,
    'xtick.labeltop': True,
    'ytick.labelright': True,
    'xtick.minor.size': 8,
})
fig, axs = plt.subplots(1, 2, figsize=(5.0, 3.0))
axs[0].plot([1, 1000], [0, 1])
axs[0].set_xscale('log')
axs[1].plot([0, 1], [0, 1])
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
