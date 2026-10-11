# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 2, figsize=(7.0, 5.0))
axs[0, 0].fill_between([0, 2, 4, 6, 8, 10], [0, 3, 1, 4, 2, 5], [9, 7, 8, 6, 9, 8], alpha=0.5)
axs[0, 0].set_xlim(4.5, 5.0)
axs[0, 0].set_ylim(4.0, 4.5)
axs[0, 0].set_title('a band far larger than the figure')
axs[0, 1].fill_between([0, 2, 4, 6, 8, 10], [4.0, 4.1, 4.0, 4.1, 4.0, 4.1], [4.2, 4.3, 4.2, 4.3, 4.2, 4.3], color='C1')
axs[0, 1].set_xlim(4.5, 5.0)
axs[0, 1].set_title('wider than the figure, not taller')
axs[1, 0].fill_between([4.6, 4.7, 4.8], [0, 30, 10], [900, 700, 800], color='C2')
axs[1, 0].set_ylim(400, 401)
axs[1, 0].set_title('taller than the figure, not wider')
axs[1, 1].scatter([1.0], [1.0], s=2000000, marker='s', alpha=0.4)
axs[1, 1].scatter([1.0], [1.0], s=400, marker='D', color='C3')
axs[1, 1].set_title('one square marker larger than the figure')

if __name__ == "__main__":
    plt.show()
