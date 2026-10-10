# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(1, 2, figsize=(5.0, 3.0))
axs[0].plot([0, 1], [1, 0])
axs[1].plot([0, 1], [0, 1])
fig.suptitle('Two panels')

if __name__ == "__main__":
    plt.show()
