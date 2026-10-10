# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 2, figsize=(5.0, 4.0))
axs[0, 0].plot([0, 1, 2], [0, 1000, 4000])
axs[0, 0].set_ylabel('counts')
axs[0, 1].set_title('A title')
axs[1, 0].set_xlabel('time (s)')
axs[1, 1].plot([0, 10], [0, 0.001])
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
