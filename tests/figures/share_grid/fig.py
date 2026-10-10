# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 2, sharex='col', sharey='row', figsize=(6.0, 4.5))
axs[0, 0].plot([0, 1], [0, 1])
axs[0, 1].plot([10, 20], [5, 50])
axs[1, 0].scatter([-3, 0.5], [100, 200])
axs[1, 1].plot([12, 31], [150, 420])
axs[1, 1].set_ylabel('inner label')
axs[0, 1].set_title('column 1')

if __name__ == "__main__":
    plt.show()
