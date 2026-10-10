# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 1, sharex=True)
axs[0].plot([0, 1, 2], [0, 1, 4])
axs[1].plot([1, 5], [3, 4])
axs[0].set_xlabel('hidden by the grid')
axs[1].set_xlabel('shared x')

if __name__ == "__main__":
    plt.show()
