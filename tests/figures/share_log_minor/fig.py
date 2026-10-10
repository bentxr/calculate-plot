# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 2, sharex=True, sharey=True, figsize=(6.0, 5.0))
axs[0, 0].plot([2, 3, 6], [2, 5, 6])
axs[1, 1].plot([2.5, 4], [3, 4])
axs[0, 1].set_xscale('log')
axs[1, 0].set_yscale('log')

if __name__ == "__main__":
    plt.show()
