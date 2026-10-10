# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(1, 2, sharey=True, figsize=(6.0, 3.0))
axs[0].plot([0, 1, 2], [1, 10, 300])
axs[1].plot([0, 1, 2], [0.02, 5, 70])
axs[1].set_yscale('log')

if __name__ == "__main__":
    plt.show()
