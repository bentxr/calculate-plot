# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(3, 2, sharex='col', sharey=True, figsize=(5.0, 5.0))
axs[0, 0].plot([0, 1, 2], [0, 1000, 4000])
axs[2, 1].errorbar([1, 2, 3], [100, 200, 150], yerr=50)
axs[1, 0].set_ylabel('middle')
axs[2, 0].set_xlabel('left column')
fig.suptitle('Shared')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
