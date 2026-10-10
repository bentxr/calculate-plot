# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 2, sharex='all', sharey='all')
axs[1, 0].set_xlim(0, 10)
axs[0, 0].plot([2, 30], [0, 1])
axs[1, 1].plot([5, 6], [-4, 7])
axs[0, 1].set_ylim(-1, 1)
axs[1, 0].plot([1, 2], [50, 60])

if __name__ == "__main__":
    plt.show()
