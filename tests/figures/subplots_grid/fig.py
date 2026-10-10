# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 2, figsize=(5.0, 4.0))
axs[0, 0].plot([0, 1, 2], [0, 1, 4])
axs[0, 1].scatter([0, 1, 2], [2, 1, 0])
axs[1, 0].set_title('lower left')
axs[1, 1].set_xlabel('x')

if __name__ == "__main__":
    plt.show()
